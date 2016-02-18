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
#include <linux/prctl.h>

#include "replicas.h"
 
#define REP_MAX 3
#define PERIOD_USEC 120 // Max time for voting in micro seconds
#define VOTER_PRIO_OFFSET 5 // Replicas run with a -5 offset

long voting_timeout;
int timer_start_index;
int timer_stop_index;
bool timer_started = false;
timestamp_t watchdog;
pid_t last_dead = -1;

// Replica related data
struct replica replicas[REP_MAX];

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
struct vote_pipe ext_pipes[PIPE_LIMIT];

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

void restart_prep(int restartee, int restarter) {
  int i;

  #ifdef TIME_RESTART_REPLICA
    timestamp_t start_restart = generate_timestamp();
  #endif // TIME_RESTART_REPLICA

  // Normally the pipes are stolen, then written back.
  // Here, we want to fill the pipes to test the impact of large messages.
  // So we will write to them, and then steal them (so that the fake data isn't processed).
  // But probably won't work if real data is there...
  // Problem: normally steal once, write twice. Need a second write... use restarted rep?
  #ifdef PIPE_SMASH
    char pipe_fill[PIPE_FILL_SIZE] = {1};
    for (i = 0; i < PIPE_LIMIT; i++) {
      if (replicas[restarter].vot_pipes[i].fd_out != 0) {
        writeBuffer(replicas[restarter].vot_pipes[i].fd_out, pipe_fill, sizeof(pipe_fill));
      }
    }  
  #endif // PIPE_SMASH

  char **restarter_buffer = (char **)malloc(sizeof(char *) * PIPE_LIMIT);
  if (restarter_buffer == NULL) {
    perror("Voter failed to malloc memory");
  }
  for (i = 0; i < PIPE_LIMIT; i++) {
    restarter_buffer[i] = (char *)malloc(sizeof(char) * MAX_VOTE_PIPE_BUFF);
    if (restarter_buffer[i] == NULL) {
      perror("Voter failed to allocat memory");
    }
  }
  int restarter_buff_count[PIPE_LIMIT] = {0};

  // Steal the pipes from healthy reps. This stops them from processing mid restart (also need to copy data to new rep)
  stealPipes(restarter, restarter_buffer, restarter_buff_count);

  // reset timer
  timer_started = false;
  restartReplica(replicas, rep_count, &sd, ext_pipes, restarter, restartee, replica_priority);

  for (i = 0; i < replicas[restarter].pipe_count; i++) {
    if (replicas[restarter].vot_pipes[i].fd_in != 0) {
      copyPipe(&(replicas[restartee].vot_pipes[i]), &(replicas[restarter].vot_pipes[i]));
      sendPipe(i, restarter); // TODO: Need to check if available?
    }
  }

  #ifndef PIPE_SMASH
    // Give the buffers back
    returnPipes(restartee, restarter_buffer, restarter_buff_count);
    returnPipes(restarter, restarter_buffer, restarter_buff_count);
    // free the buffers
    for (i = 0; i < PIPE_LIMIT; i++) {
      free(restarter_buffer[i]);
    }
    free(restarter_buffer);
  #endif // !PIPE_SMASH

  // The second write
  #ifdef PIPE_SMASH
    for (i = 0; i < PIPE_LIMIT; i++) {
      if (replicas[restarter].vot_pipes[i].fd_out != 0) {
        writeBuffer(replicas[restarter].vot_pipes[i].fd_out, pipe_fill, sizeof(pipe_fill));
      }
    }  
  #endif // PIPE_SMASH

  #ifdef TIME_RESTART_REPLICA
    timestamp_t end_restart = generate_timestamp();
    printf("Restart time elapsed usec (%lf)\n", diff_time(end_restart, start_restart, CPU_MHZ));
  #endif // TIME_RESTART_REPLICA

  // Clean up by stealing the extra write. Not timed.
  #ifdef PIPE_SMASH
    if (restarter_buffer == NULL) {
      perror("Voter failed to malloc memory");
    }
    for (i = 0; i < PIPE_LIMIT; i++) {
      restarter_buffer[i] = (char *)malloc(sizeof(char) * MAX_VOTE_PIPE_BUFF);
      if (restarter_buffer[i] == NULL) {
        perror("Voter failed to allocat memory");
      }
    }

    stealPipes(restarter, restarter_buffer, restarter_buff_count);
    // free the buffers
    for (i = 0; i < PIPE_LIMIT; i++) {
      free(restarter_buffer[i]);
    }
    free(restarter_buffer);
  #endif // PIPE_SMASH

  return;
}

void voterRestartHandler(void) {
  // Timer went off, so the timer_stop_index is the pipe which is awaiting a rep
  int p_index;
  debug_print("Caught Exec / Control loop error (%s)\n", controller_name);

  switch (rep_type) {
    case SMR: {
      #ifdef TIME_RESTART_REPLICA
        timestamp_t start_restart = generate_timestamp();
      #endif // TIME_RESTART_REPLICA

      // Need to cold restart the replica
      last_dead = replicas[0].pid;
      cleanupReplica(replicas, 0);

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
      int restartee = behindRep(replicas, rep_count, timer_stop_index);
      //last_dead = replicas[restartee].pid;
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

  // TODO: Would this trip for a SMR component that faulted?
  if (rep_type == SMR) { // Detect replica that self-kills, has no timer (Load does this due to memory leak)
    #ifdef TIME_WAITPID
      timestamp_t start_restart = generate_timestamp();
    #endif // TIME_WAITPID

    // can only waitpid for children (unless subreaper is used (prctl is not POSIX compliant)).
    int exit_pid = waitpid(-1, NULL, WNOHANG); // Seems to take a while for to clean up zombies
      
    #ifdef TIME_WAITPID
      timestamp_t end_restart = generate_timestamp();
      if (exit_pid > 0 && exit_pid != last_dead) {
        printf("Waitpid for %d (%s) took usec (%lf)\n", exit_pid, REP_TYPE_T[rep_type], diff_time(end_restart, start_restart, CPU_MHZ));
      } else {
        //printf("No zombie took (%lld)\n", end_restart - start_restart);
      }
    #endif // TIME_WAITPID

    if (exit_pid > 0 && exit_pid != last_dead) {
      debug_print("PID %d exited on its own.\n", exit_pid);
      voterRestartHandler();
      timer_started = false;
    }
  }

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
      debug_print("Restart handler called, %s is %ld late\n", controller_name, remaining);
      voterRestartHandler();
    }
  }

  // See if any of the read pipes have anything
  FD_ZERO(&select_set);
  // Check external in pipes
  bool check_inputs = checkSync();

  if (check_inputs) {
    for (p_index = 0; p_index < pipe_count; p_index++) {
      if (ext_pipes[p_index].fd_in != 0) {
        int e_pipe_fd = ext_pipes[p_index].fd_in;
        FD_SET(e_pipe_fd, &select_set);
      }
    }
  } else if (voter_priority < 5 && ! timer_started) {
    // non-RT controller is now lagging behind.
    timer_started = true;
    watchdog = generate_timestamp();
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
          ext_pipes[p_index].buff_count = TEMP_FAILURE_RETRY(read(read_fd, ext_pipes[p_index].buffer, MAX_VOTE_PIPE_BUFF));
          if (ext_pipes[p_index].buff_count > 0) { // TODO: read may still have been interrupted
            processData(&(ext_pipes[p_index]), p_index);
          } else if (ext_pipes[p_index].buff_count < 0) {
            printf("Voter - Controller %s pipe %d\n", controller_name, p_index);
            perror("Voter - read error on external pipe");
          } else {
            printf("Voter - Controller %s pipe %d\n", controller_name, p_index);
            perror("Voter - read == 0 on external pipe");
          }
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
  int retval = TEMP_FAILURE_RETRY(write(fd_out, buffer, buff_count));
  if (retval == buff_count) {
    // success, do nothing
  } else if (retval > 0) { // TODO: resume write? 
    printf("Voter for %s, pipe %d, bytes written: %d\texpected: %d\n", controller_name, fd_out, retval, buff_count);
    perror("Voter wrote partial message");
  } else if (retval < 0) {
    printf("Voter for %s failed write fd: %d\n", controller_name, fd_out);
    perror("Voter write");
  } else {
    printf("Voter wrote == 0 for %s fd: %d\n", controller_name, fd_out);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process data
void processData(struct vote_pipe *pipe, int pipe_index) {
  int r_index;

  if (pipe_index == timer_start_index) {
    if (!timer_started) {
      timer_started = true;
      watchdog = generate_timestamp();
    }
  }

  balanceReps(replicas, rep_count, replica_priority);

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

  if (pipe_num == timer_stop_index) {  
    // reset the timer
    timer_started = false;
  }

  for (r_index = 0; r_index < rep_count; r_index++) {
    if (replica_num == r_index) {
      int retval;
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
        printf("Voting disagreement: caught SDC in DMR but can't do anything about it.\n");
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
            
            debug_print("Caught SDC: %s : %d\n", controller_name, replicas[restartee].pid);
            if (DEBUG_PRINT) {
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
            }
            restart_prep(restartee, r_index);
          } else {
            // If all agree, send and be happy. Otherwise the send is done as part of the restart process
            sendPipe(pipe_num, r_index);
          }
          return;
        } 
      }

      printf("VoterD: TMR no two replicas agreed.\n");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process output from replica; vote on it
void processFromRep(int replica_num, int pipe_num) {
  // read from pipe
  if (pipeToBuff(&(replicas[replica_num].vot_pipes[pipe_num])) == 0) {
    balanceReps(replicas, rep_count, replica_priority);
    checkSDC(pipe_num);
  } else {
    printf("Voter - Controller %s, rep %d, pipe %d\n", controller_name, replica_num, pipe_num);
    perror("Voter - read problem on internal pipe");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterD(void) {
  replica_priority = voter_priority - VOTER_PRIO_OFFSET;

  // Setup fd server
  createFDS(&sd, controller_name);
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
  int in, out, timed;

  sscanf(serial, "%m[^:]:%d:%d:%d", &rep_info, &in, &out, &timed);
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
    puts("Usage: VoterD <controller_name> <rep_type> <timeout> <priority> <fd_in:fd_out:time> <...>");
    return -1;
  } else {
    for (i = 0; (i < argc - required_args && i < PIPE_LIMIT); i++) {
      parsePipe(argv[i + required_args], &ext_pipes[pipe_count]); // TODO: WRONG! Maybe. Should ignore non-numbers to deserialize?
      pipe_count++;
    }
    if (pipe_count >= PIPE_LIMIT) {
      printf("VoterD: Raise pipe limit.\n");
    }
  
    // Need to have a similar setup to associate vote pipe with input?
    for (i = 0; i < pipe_count; i++) {
      if (ext_pipes[i].timed) {
        if (ext_pipes[i].fd_in != 0) {
          timer_start_index = i;
        } else {
          timer_stop_index = i;
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

  sleep(1);

  while(1) {
    doOneUpdate();
  }

  return 0;
}
