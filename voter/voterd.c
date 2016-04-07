/*
 * Voter that is able to start and connect to other voters, and maybe
 * even be generic
 *
 * Author - James Marshall
 */

#include "controller.h"
#include "tas_time.h"

#include <malloc.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include "replicas.h"
 
#define REP_MAX 3
#define PERIOD_USEC 120 // Max time for voting in micro seconds
#define VOTER_PRIO_OFFSET 5 // Replicas run with a -5 offset

long voting_timeout;
int current_timer = 0;
int timer_start_index[2]; // Should have a higher max than two...
int timer_stop_index[2];
bool timer_started = false;
timestamp_t watchdog;

// Replica related data
struct replica replicas[REP_MAX]; // TODO: malloc

// TAS Stuff
int voter_priority;
int replica_priority;

// FD server
struct server_data sd;

replication_t rep_type;
int rep_count;
char* controller_name;
// pipes to external components (not replicas)
int pipe_count = 0;
struct vote_pipe *ext_pipes;

// Functions!
int initVoterD(void);
int parseArgs(int argc, const char **argv);
void doOneUpdate(void);
void processData(struct vote_pipe *pipe, int pipe_index);
void resetVotingState(int pipe_num);
void sendPipe(int pipe_num, int replica_num);
void stealPipes(int rep_num, char **buffer, int *buff_count);
void returnPipes(int rep_num, char **buffer, int *buff_count);
void checkSDC(int pipe_num);
void processFromRep(int replica_num, int pipe_num);
void writeBuffer(int fd_out, unsigned char* buffer, int buff_count);

timestamp_t start_time;
void reportRUsageHandler(int sign, siginfo_t *si, void *unused) {
  // getrusage isn't in the safe list... so we'll see.
  timestamp_t current = generate_timestamp();
  struct rusage usage;

  printf("Report RUsage VoterD - (%d)\n", getpid());
  printf("\tTime since process init: %lf\n", diff_time(current, start_time, CPU_MHZ));

  if (getrusage(RUSAGE_SELF, &usage) < 0) {
    perror("Controller getrusage failed");
  } else {
    printf("\tUtime:\t%ld Sec %ld uSec\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
    printf("\tStime:\t%ld Sec %ld uSec\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);

    printf("\tRSS:\t %ld %ld %ld %ld\n", usage.ru_maxrss, usage.ru_ixrss, usage.ru_idrss, usage.ru_isrss);
    printf("\tFLT:\t %ld %ld\n", usage.ru_minflt, usage.ru_majflt);
  }
}

void restart_prep(int restartee, int restarter) {
  int i;

  #ifdef TIME_RESTART_REPLICA
    timestamp_t start_restart = generate_timestamp();
  #endif // TIME_RESTART_REPLICA

  char **restarter_buffer = (char **)malloc(sizeof(char *) * pipe_count);
  if (restarter_buffer == NULL) {
    perror("Voter failed to malloc memory");
  }
  int restarter_buff_count[pipe_count];
  for (i = 0; i < pipe_count; i++) {
    restarter_buff_count[i] = 0;
    restarter_buffer[i] = (char *)malloc(sizeof(char) * MAX_VOTE_PIPE_BUFF);
    if (restarter_buffer[i] == NULL) {
      perror("Voter failed to allocat memory");
    }
  }

  // Steal the pipes from healthy rep. This stops them from processing mid restart (also need to copy data to new rep)
  stealPipes(restarter, restarter_buffer, restarter_buff_count);

  // reset timer
  timer_started = false;
  restartReplica(replicas, rep_count, &sd, ext_pipes, restarter, restartee, replica_priority);

  for (i = 0; i < pipe_count; i++) {
    if (replicas[restarter].vot_pipes[i].fd_in != 0) {
      copyPipe(&(replicas[restartee].vot_pipes[i]), &(replicas[restarter].vot_pipes[i]));
      sendPipe(i, restarter); // TODO: Need to check if available?
    }
  }

  // Give the buffers back
  returnPipes(restartee, restarter_buffer, restarter_buff_count);
  returnPipes(restarter, restarter_buffer, restarter_buff_count);
  // free the buffers
  for (i = 0; i < PIPE_LIMIT; i++) {
    free(restarter_buffer[i]);
  }
  free(restarter_buffer);

  #ifdef TIME_RESTART_REPLICA
    timestamp_t end_restart = generate_timestamp();
    printf("Restart time elapsed usec (%lf)\n", diff_time(end_restart, start_restart, CPU_MHZ));
  #endif // TIME_RESTART_REPLICA

  return;
}

void voterRestartHandler(void) {
  // Timer went off, so the timer_stop_index is the pipe which is awaiting a rep
  int p_index;
  debug_print("VoterD(%s) Caught Exec / Control loop error\n", controller_name);

  switch (rep_type) {
    case SMR: {
      #ifdef TIME_RESTART_REPLICA
        timestamp_t start_restart = generate_timestamp();
      #endif // TIME_RESTART_REPLICA

      // Need to cold restart the replica
      cleanupReplica(replicas, 0);

      waitpid(-1, NULL, WNOHANG); // Only you can prevent zombie outbreaks.

      startReplicas(replicas, rep_count, &sd, controller_name, ext_pipes, pipe_count, replica_priority);
      
      // Resend last data
      for (p_index = 0; p_index < pipe_count; p_index++) {
        int read_fd = ext_pipes[p_index].fd_in;
        if (read_fd != 0) {
          processData(&(ext_pipes[p_index]), p_index);    
        }
      }

      #ifdef TIME_RESTART_REPLICA
        timestamp_t end_restart = generate_timestamp();
        printf("Restart time elapsed usec (%lf)\n", diff_time(end_restart, start_restart, CPU_MHZ));
      #endif // TIME_RESTART_REPLICA

      break;
    }
    case DMR: { // Intentional fall-through to TMR
    }
    case TMR: {
      // The failed rep should be the one behind on the timer pipe
      int restartee = behindRep(replicas, rep_count, timer_stop_index[current_timer]);
      int restarter = (restartee + (rep_count - 1)) % rep_count;
      debug_print("\tPID: %d\n", replicas[restartee].pid);
      restart_prep(restartee, restarter);
      break;
    }

    return;
  }
}

// Steal the pipe contents from a single replica
// buff_count and buffer should already be alocated.
void stealPipes(int rep_num, char **buffer, int *buff_count) {
  int i = 0;

  struct timeval select_timeout;
  fd_set select_set;
  select_timeout.tv_sec = 0;
  select_timeout.tv_usec = 0;

  FD_ZERO(&select_set);
  for (i = 0; i < replicas[rep_num].pipe_count; i++) {
    if (replicas[rep_num].voter_rep_in_copy[i] != 0) {
      FD_SET(replicas[rep_num].voter_rep_in_copy[i], &select_set);
    }
  }

  int retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
  if (retval > 0) { // Copy buffers
    for (i = 0; i < replicas[rep_num].pipe_count; i++) {
      if (FD_ISSET(replicas[rep_num].voter_rep_in_copy[i], &select_set)) {
        buff_count[i] = read(replicas[rep_num].voter_rep_in_copy[i], buffer[i], MAX_VOTE_PIPE_BUFF);
        if (buff_count[i] < 0) {
          perror("Voter error stealing pipe");
        }
      }
    }
  }  
}

// replace pipe buffers in a replica. Does NOT free the buffer (so the same one can be copied again)
void returnPipes(int rep_num, char **buffer, int *buff_count) {  
  int i = 0;
  for (i = 0; i < replicas[rep_num].pipe_count; i++) {
    if (buff_count[i] > 0) {
      writeBuffer(replicas[rep_num].vot_pipes[i].fd_out, buffer[i], buff_count[i]);
    }
  }
}

bool checkSync(void) {
  int r_index, p_index;
  bool nsync = true;

  // check each that for each pipe, each replica has the same number of votes
  for (p_index = 0; p_index < pipe_count; p_index++) {
    int votes = replicas[0].vot_pipes[p_index].buff_count;
    for (r_index = 1; r_index < rep_count; r_index++) {
      if (votes != replicas[r_index].vot_pipes[p_index].buff_count) {
        nsync = false;
      }
    }
  }
  return nsync;
}

void doOneUpdate(void) {
  int p_index, r_index;
  int retval = 0;

  struct timeval select_timeout;
  fd_set select_set;

  select_timeout.tv_sec = 0;
  select_timeout.tv_usec = 50000;

  if (timer_started) {
    timestamp_t current = generate_timestamp();
    long remaining = voting_timeout - diff_time(current, watchdog, CPU_MHZ);
    // debug_print("Diff time: %f = diff_time(%llu, %llu, %f); %ld\n", diff_time(current, watchdog, CPU_MHZ), current, watchdog, CPU_MHZ, remaining);
    if (remaining > 0) {
      select_timeout.tv_sec = 0;
      select_timeout.tv_usec = remaining;
    } else {
      debug_print("VoterD(%s) Restart handler called, %ld late\n", controller_name, remaining);
      voterRestartHandler();
    }
  }

  // See if any of the read pipes have anything
  FD_ZERO(&select_set);

  // Check external in pipes
  bool check_inputs = checkSync();
  if (check_inputs) {
    if (!timer_started) {
      for (p_index = 0; p_index < pipe_count; p_index++) {
        if (ext_pipes[p_index].fd_in != 0) {
          int e_pipe_fd = ext_pipes[p_index].fd_in;
          FD_SET(e_pipe_fd, &select_set);
        }
      }
    }
  }

  // Check pipes from replicas
  for (p_index = 0; p_index < pipe_count; p_index++) {
    for (r_index = 0; r_index < rep_count; r_index++) {
      int rep_pipe_fd = replicas[r_index].vot_pipes[p_index].fd_in;
      if (rep_pipe_fd != 0) {
        FD_SET(rep_pipe_fd, &select_set);      
      }
    }
  }

  // This will wait at least timeout until return. Returns earlier if something has data.
  retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);

  if (retval > 0) {    
    // Check for data from external sources
    for (p_index = 0; p_index < pipe_count; p_index++) {
      int read_fd = ext_pipes[p_index].fd_in;
      if (read_fd != 0) {
        if (FD_ISSET(read_fd, &select_set)) {
          ext_pipes[p_index].buff_count = read(read_fd, ext_pipes[p_index].buffer, MAX_VOTE_PIPE_BUFF);
          if (ext_pipes[p_index].buff_count > 0) { // TODO: read may still have been interrupted
            processData(&(ext_pipes[p_index]), p_index);
          } else if (ext_pipes[p_index].buff_count < 0) {
            debug_print("VoterD(%s) read error on external pipe - %d\n", controller_name, p_index);
          } else {
            debug_print("VoterD(%s) read == 0 on external pipe - %d\n", controller_name, p_index);
          }

	  break; // Hmmmmmmm
        }
      }
    }

    // Check all replicas for data
    for (p_index = 0; p_index < pipe_count; p_index++) {
      for (r_index = 0; r_index < rep_count; r_index++) {
        struct vote_pipe* curr_pipe = &(replicas[r_index].vot_pipes[p_index]);
        if (curr_pipe->fd_in !=0) {
          if (FD_ISSET(curr_pipe->fd_in, &select_set)) {
            processFromRep(r_index, p_index);
          }
        }
      }
    }
  }
}

void writeBuffer(int fd_out, unsigned char* buffer, int buff_count) {
  int retval = write(fd_out, buffer, buff_count);
  if (retval == buff_count) {
    // success, do nothing
  } else if (retval > 0) { // TODO: resume write? 
    debug_print("VoterD(%s) wrote partial message, pipe %d, bytes written: %d\texpected: %d\n", controller_name, fd_out, retval, buff_count);
  } else if (retval < 0) {
    debug_print("VoterD(%s) failed write fd: %d\n", controller_name, fd_out);
  } else {
    debug_print("VoterD(%s) wrote == 0 fd: %d\n", controller_name, fd_out);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process data
void processData(struct vote_pipe *pipe, int pipe_index) {
  int r_index;

  if (!timer_started) {
    if (pipe_index == timer_start_index[0]) {
      current_timer = 0;
      timer_started = true;
      watchdog = generate_timestamp();
    } else if (pipe_index == timer_start_index[1]) {
      current_timer = 1;
      timer_started = true;
      watchdog = generate_timestamp();
    }
  }

  balanceReps(replicas, rep_count, replica_priority);

  // printf("VoterD(%s) writing %d bytes to pipes\n", controller_name, pipe->buff_count);
  for (r_index = 0; r_index < rep_count; r_index++) {
    writeBuffer(replicas[r_index].vot_pipes[pipe_index].fd_out, pipe->buffer, pipe->buff_count);
  }
}

void sendPipe(int pipe_num, int replica_num) {
  int r_index;
  int bytes_avail = bytesReady(replicas, rep_count, pipe_num);
  if (bytes_avail == 0) {
    return;
  }

  if (pipe_num == timer_stop_index[0] || pipe_num == timer_stop_index[1]) {
    // reset the timer
    timer_started = false;
  }

  for (r_index = 0; r_index < rep_count; r_index++) {
    if (replica_num == r_index) {
      int retval; // TODO: error?
      retval = buffToPipe(&(replicas[r_index].vot_pipes[pipe_num]), ext_pipes[pipe_num].fd_out, bytes_avail);
    } else {
      fakeToPipe(&(replicas[r_index].vot_pipes[pipe_num]), bytes_avail);
    }
  }
}

void checkSDC(int pipe_num) {
  int r_index;
  int bytes_avail = bytesReady(replicas, rep_count, pipe_num);
  if (bytes_avail == 0) {
    return;
  }

  switch (rep_type) {
    case SMR:
      // Only one rep, so pretty much have to trust it
      sendPipe(pipe_num, 0);
      return;
    case DMR:
      // Can detect, and check what to do
      if (compareBuffs(&(replicas[0].vot_pipes[pipe_num]), &(replicas[1].vot_pipes[pipe_num]), bytes_avail) != 0) {
        debug_print("VoterD(%s) caught SDC in DMR but can't do anything about it.\n", controller_name);
	debug_print("\tPipe_num %d, Bytes available %d, buff_counts: %d, %d\n", pipe_num, bytes_avail, replicas[0].vot_pipes[pipe_num].buff_count, replicas[1].vot_pipes[pipe_num].buff_count);

            #ifdef DEBUG_PRINT
              // Create typed pipes for meta data
              struct typed_pipe print_pipesA[pipe_count];
              struct typed_pipe print_pipesB[pipe_count];
              convertVoteToTyped(replicas[0].vot_pipes, pipe_count, print_pipesA);
              convertVoteToTyped(replicas[1].vot_pipes, pipe_count, print_pipesB);
              
              // Copy the buffer over
              char *buffer_A = (char *)malloc(sizeof(char) * MAX_VOTE_PIPE_BUFF);
              char *buffer_B = (char *)malloc(sizeof(char) * MAX_VOTE_PIPE_BUFF);
              copyBuffer(&(replicas[0].vot_pipes[pipe_num]), buffer_A, bytes_avail);
              copyBuffer(&(replicas[1].vot_pipes[pipe_num]), buffer_B, bytes_avail);

              // print them out.
              printBuffer(&(print_pipesA[pipe_num]), buffer_A, bytes_avail);
              printBuffer(&(print_pipesB[pipe_num]), buffer_B, bytes_avail);

              free(buffer_A);
              free(buffer_B);
            #endif /* DEBUG_PRINT */


      }

      sendPipe(pipe_num, 0);
      return;
    case TMR:
      // Send the solution that at least two agree on
      // TODO: What if buff_count is off?
      for (r_index = 0; r_index < rep_count; r_index++) {
        if (compareBuffs(&(replicas[r_index].vot_pipes[pipe_num]), &(replicas[(r_index + 1) % rep_count].vot_pipes[pipe_num]), bytes_avail) == 0) {
          // If the third doesn't agree, it should be restarted.
          if (compareBuffs(&(replicas[r_index].vot_pipes[pipe_num]), &(replicas[(r_index + 2) % rep_count].vot_pipes[pipe_num]), bytes_avail) != 0) {  
            int restartee = (r_index + 2) % rep_count;
            
            debug_print("VoterD(%s) Caught SDC: %d\n", controller_name, replicas[restartee].pid);
            #ifdef DEBUG_PRINT
              // print all three or just two?

              // Create typed pipes for meta data
              struct typed_pipe print_pipesA[pipe_count];
              struct typed_pipe print_pipesB[pipe_count];
              convertVoteToTyped(replicas[r_index].vot_pipes, pipe_count, print_pipesA);
              convertVoteToTyped(replicas[(r_index + 2) % rep_count].vot_pipes, pipe_count, print_pipesB);
              
              // Copy the buffer over
              char *buffer_A = (char *)malloc(sizeof(char) * MAX_VOTE_PIPE_BUFF);
              char *buffer_B = (char *)malloc(sizeof(char) * MAX_VOTE_PIPE_BUFF);
              copyBuffer(&(replicas[r_index].vot_pipes[pipe_num]), buffer_A, bytes_avail);
              copyBuffer(&(replicas[(r_index + 2) % rep_count].vot_pipes[pipe_num]), buffer_B, bytes_avail);

              // print them out.
              printBuffer(&(print_pipesA[pipe_num]), buffer_A, bytes_avail);
              printBuffer(&(print_pipesB[pipe_num]), buffer_B, bytes_avail);

              free(buffer_A);
              free(buffer_B);
            #endif /* DEBUG_PRINT */
            restart_prep(restartee, r_index);
          } else {
            // If all agree, send and be happy. Otherwise the send is done as part of the restart process
            sendPipe(pipe_num, r_index);
          }
          return;
        } 
      }

      debug_print("VoterD(%s): TMR no two replicas agreed.\n", controller_name);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process output from replica; vote on it
void processFromRep(int replica_num, int pipe_num) {
  if (!timer_started) {
    if (pipe_num == timer_stop_index[0]) {
      current_timer = 0;
      timer_started = true;
      watchdog = generate_timestamp();
    } else if (pipe_num == timer_stop_index[1]) {
      current_timer = 1;
      timer_started = true;
      watchdog = generate_timestamp();
    }
  }

  // read from pipe
  if (pipeToBuff(&(replicas[replica_num].vot_pipes[pipe_num])) == 0) {
    balanceReps(replicas, rep_count, replica_priority);
    checkSDC(pipe_num);
  } else {
    debug_print("VoterD(%s) - read problem on internal pipe. rep %d, pipe %d\n", controller_name, replica_num, pipe_num);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterD(void) {
  // register for rusage signal and take start time.
  struct sigaction sa;
  start_time = generate_timestamp();

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = reportRUsageHandler;
  if (sigaction(RRUSAGE_SIGNAL, &sa, NULL) == -1) {
    debug_print("Failed to register VoterM for the report rusage handler.\n");
    return -1;
  }

  replica_priority = voter_priority - VOTER_PRIO_OFFSET;

  // Setup fd server
  if (createFDS(&sd, controller_name) < 0) {
    debug_print("VoterD(%s) Failed to create FD server\n", controller_name);
  }
  startReplicas(replicas, rep_count, &sd, controller_name, ext_pipes, pipe_count, replica_priority);

  InitTAS(VOTER_PIN, voter_priority); // IMPORTANT: Should be after forking replicas to subvert CoW

  // TODO: I did this to force page faults... but I think there was a different issue.
  // This section may no longer be necessary. Need to check with the page fault tests.
  int p_index, r_index;

  for (p_index = 0; p_index < pipe_count; p_index++) {
    int read_fd = ext_pipes[p_index].fd_in;
    if (read_fd != 0) {  // Causes page faults
      ext_pipes[p_index].buff_count = read(read_fd, ext_pipes[p_index].buffer, 0);
    }
  }

  // Check all replicas for data
  for (r_index = 0; r_index < rep_count; r_index++) {
    for (p_index = 0; p_index < replicas[r_index].pipe_count; p_index++) {
      struct vote_pipe* curr_pipe = &(replicas[r_index].vot_pipes[p_index]);
      if (curr_pipe->fd_in !=0) {
        curr_pipe->buff_count = read(curr_pipe->fd_in, curr_pipe->buffer, 0);
      }
    }
  }

  debug_print("Initializing VoterD(%s)\n", controller_name);

  return 0;
}

void parsePipe(const char* serial, struct vote_pipe* pipe) {
  char *rep_info;
  int in, out, timed, i, str_length;

  for (i = 0; i < 100; i++) {
    if (serial[i] == ':') {
      sscanf(&(serial[i]), ":%d:%d:%d", &in, &out, &timed);
      str_length = i;
      break;
    }
  }
  rep_info = (char*) malloc(sizeof(char) * str_length);
  for (i = 0; i < str_length; i++) {
    rep_info[i] = serial[i];
  }

  pipe->rep_info = rep_info;
  pipe->fd_in = in;
  pipe->fd_out = out;
  pipe->timed = timed;
}

int parseArgs(int argc, const char **argv) {
  int i;
  int required_args = 5; // voter name, controller name, rep_type, timeout and priority
  controller_name = (char*) (argv[1]);
  rep_type = reptypeToEnum((char*)(argv[2]));
  rep_count = reptypeToCount(rep_type);
  voting_timeout = atoi(argv[3]);
  voter_priority = atoi(argv[4]);
  if (voting_timeout == 0) {
    voting_timeout = PERIOD_USEC;
  }

  if (argc < required_args) { 
    puts("Usage: VoterD <controller_name> <rep_type> <timeout> <priority> <rep_info:fd_in:fd_out:time> <...>");
    return -1;
  } else {
    for (i = 0; (i < argc - required_args && i < PIPE_LIMIT); i++) {
      pipe_count++;
    }
    if (pipe_count >= PIPE_LIMIT) {
      debug_print("VoterD(%s) Raise pipe limit.\n", controller_name);
    }

    ext_pipes = (struct vote_pipe *) malloc(sizeof(struct vote_pipe) * pipe_count);
    for (i = 0; (i < argc - required_args && i < PIPE_LIMIT); i++) {
      parsePipe(argv[i + required_args], &ext_pipes[i]); // TODO: WRONG! Maybe. Should ignore non-numbers to deserialize?
    }

    for (i = 0; i < rep_count; i++) {
      replicas[i].vot_pipes = (struct vote_pipe *) malloc(sizeof(struct vote_pipe) * pipe_count);
      replicas[i].voter_rep_in_copy = (int *) malloc(sizeof(int) * pipe_count);
      replicas[i].rep_pipes = (struct vote_pipe *) malloc(sizeof(struct vote_pipe) * pipe_count);
    }

    // Need to have a similar setup to associate vote pipe with input?
    int c_in_pipe = 0, c_out_pipe = 0;
    for (i = 0; i < pipe_count; i++) {
      if (ext_pipes[i].timed) {
        if (ext_pipes[i].fd_in != 0) {
          timer_start_index[c_in_pipe] = i;
          c_in_pipe++;
        } else {
          timer_stop_index[c_out_pipe] = i;
          c_out_pipe++;
        }
      }
    }
  }

  return 0;
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initVoterD() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  while(1) {
    doOneUpdate();
  }

  return 0;
}
