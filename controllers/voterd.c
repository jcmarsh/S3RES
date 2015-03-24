/*
 * Voter that is able to start and connect to other voters, and maybe
 * even be generic
 *
 * Author - James Marshall
 */

#include "../include/controller.h"

#include <assert.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <linux/prctl.h>
#include <time.h>

#include "../include/replicas.h"
#include "../include/fd_server.h"
#include "../include/scheduler.h"

#define REP_MAX 3
#define PERIOD_NSEC 120000 // Max time for voting in nanoseconds (120 micro seconds)
#define VOTER_PRIO_OFFSET 5 // Replicas run with a +20 offset (low prio)

long voting_timeout;
int timer_start_index;
int timer_stop_index;
bool timer_started;
timestamp_t watchdog;

// Replica related data
struct replica replicas[REP_MAX];

// TAS Stuff
int voter_priority;

// FD server
struct server_data sd;

replication_t rep_type;
int rep_count;
char* controller_name;
// pipes to external components (not replicas)
int pipe_count = 0;
struct typed_pipe ext_pipes[PIPE_LIMIT];

// Functions!
int initVoterD(void);
int parseArgs(int argc, const char **argv);
int main(int argc, const char **argv);
void doOneUpdate(void);
void processData(struct typed_pipe *pipe, int pipe_index);
void resetVotingState(int pipe_num);
void initVotingState(void);
void sendPipe(int pipe_num, int replica_num);
void stealBuffers(int rep_num, char **buffer, int *buff_count);
void returnBuffers(int rep_num, char **buffer, int *buff_count);
void checkSDC(int pipe_num);
void processFromRep(int replica_num, int pipe_num);
void restartReplica(int restarter, int restartee);
void cleanupReplica(int rep_index);
void writeBuffer(int fd_out, char* buffer, int buff_count);

void startReplicas(void) {
  int i;
  initReplicas(replicas, rep_count, controller_name, voter_priority + VOTER_PRIO_OFFSET);
  createPipes(replicas, rep_count, ext_pipes, pipe_count);
  forkReplicas(replicas, rep_count);
  for (i = 0; i < rep_count; i++) {
    if (acceptSendFDS(&sd, &(replicas[i].pid), replicas[i].rep_pipes, replicas[i].pipe_count) < 0) {
      printf("VoterD acceptSendFDS call failed\n");
      exit(-1);
    }
  }
}

// return the index of the rep that is furthest behind in voting
int behindRep(int pipe_num) {
  int r_index = 0;
  int mostBehind = r_index;
  for (r_index = 0; r_index < rep_count; r_index++) {
    if (replicas[r_index].voted[pipe_num] < replicas[mostBehind].voted[pipe_num]) {
      mostBehind = r_index;
    }
  }
  return mostBehind;
}

int aheadRep(int pipe_num) {
  int r_index = 0;
  int mostAhead = r_index;
  for (r_index = 0; r_index < rep_count; r_index++) {
    if (replicas[r_index].voted[pipe_num] > replicas[mostAhead].voted[pipe_num]) {
      mostAhead= r_index;
    }
  }
  return mostAhead;
}

void voterRestartHandler(void) {
  // Timer went off, so the timer_stop_index is the pipe which is awaiting a rep
  int p_index;

  // TODO: A lot of replication here, especially with SDC handling.
  switch (rep_type) {
    case SMR: {
      // Need to cold restart the replica
      cleanupReplica(0);

      startReplicas();
      
      // Resend last data
      for (p_index = 0; p_index < pipe_count; p_index++) {
        int read_fd = ext_pipes[p_index].fd_in;
        if (read_fd != 0) {
          processData(&(ext_pipes[p_index]), p_index);    
        }
      }

      break;
    }
    case DMR: {
      // The failed rep should be the one behind on the timer pipe
      int restartee = behindRep(timer_stop_index);
      int restarter = (restartee + (rep_count - 1)) % rep_count;
      
      int i;
      char **restarter_buffer = (char **)malloc(sizeof(char *) * PIPE_LIMIT);
      if (restarter_buffer == NULL) {
        perror("Voter failed to malloc memory");
      }
      for (i = 0; i < PIPE_LIMIT; i++) {
        restarter_buffer[i] = (char *)malloc(sizeof(char) * MAX_PIPE_BUFF);
        if (restarter_buffer[i] == NULL) {
          perror("Voter failed to allocat memory");
        }
      }
      int restarter_buff_count[PIPE_LIMIT] = {0};

      // Steal the buffers from healthy reps. This stops them from processing mid restart
      stealBuffers(restarter, restarter_buffer, restarter_buff_count);

      restartReplica(restarter, restartee);

      // Give the buffers back
      returnBuffers(restartee, restarter_buffer, restarter_buff_count);
      returnBuffers(restarter, restarter_buffer, restarter_buff_count);
      // free the buffers
      for (i = 0; i < PIPE_LIMIT; i++) {
        free(restarter_buffer[i]);
      }
      free(restarter_buffer);
      break;
    }
    case TMR: {// the semicolon is needed becasue C.
      // The failed rep should be the one behind on the timer pipe
      int restartee = behindRep(timer_stop_index);
      int restarter = (restartee + (rep_count - 1)) % rep_count;
      int other_rep = (restarter + (rep_count - 1)) % rep_count;

      int i;
      char **restarter_buffer = (char **)malloc(sizeof(char *) * PIPE_LIMIT);
      if (restarter_buffer == NULL) {
        perror("Voter failed to malloc memory");
      }
      char **other_rep_buffer = (char **)malloc(sizeof(char *) * PIPE_LIMIT);
      if (other_rep_buffer == NULL) {
        perror("Voter failed to malloc memory");
      }
      for (i = 0; i < PIPE_LIMIT; i++) {
        restarter_buffer[i] = (char *)malloc(sizeof(char) * MAX_PIPE_BUFF);
        if (restarter_buffer[i] == NULL) {
          perror("Voter failed to allocat memory");
        }
        other_rep_buffer[i] = (char *)malloc(sizeof(char) * MAX_PIPE_BUFF);
        if (other_rep_buffer[i] == NULL) {
          perror("Voter failed to allocat memory");
        }
      }
      int restarter_buff_count[PIPE_LIMIT] = {0};
      int other_rep_buff_count[PIPE_LIMIT] = {0};

      // Steal the buffers from healthy reps. This stops them from processing mid restart
      stealBuffers(restarter, restarter_buffer, restarter_buff_count);
      stealBuffers(other_rep, other_rep_buffer, other_rep_buff_count);

      restartReplica(restarter, restartee);

      // Give the buffers back
      returnBuffers(restartee, restarter_buffer, restarter_buff_count);
      returnBuffers(restarter, restarter_buffer, restarter_buff_count);
      returnBuffers(other_rep, other_rep_buffer, other_rep_buff_count);
      // free the buffers
      for (i = 0; i < PIPE_LIMIT; i++) {
        free(restarter_buffer[i]);
        free(other_rep_buffer[i]);
      }
      free(restarter_buffer);
      free(other_rep_buffer);
      break;
    }

    return;
  }
}

void cleanupReplica(int rep_index) {
  int i;
  // Kill old replica
  kill(replicas[rep_index].pid, SIGKILL); // Make sure it is dead.
  //waitpid(-1, NULL, WNOHANG); // cleans up the zombie // Actually doesn't // Well, now it does.
  
  // cleanup replica data structure
  for (i = 0; i < replicas[rep_index].pipe_count; i++) {
    if (replicas[rep_index].vot_pipes[i].fd_in > 0) {
      close(replicas[rep_index].vot_pipes[i].fd_in);
    }
    if (replicas[rep_index].vot_pipes[i].fd_out > 0) {
      close(replicas[rep_index].vot_pipes[i].fd_out);
    }
    if (replicas[rep_index].rep_pipes[i].fd_in > 0) {
      close(replicas[rep_index].rep_pipes[i].fd_in);
    }
    if (replicas[rep_index].rep_pipes[i].fd_out > 0) {
      close(replicas[rep_index].rep_pipes[i].fd_out);
    }
    if (replicas[rep_index].voter_rep_in_copy[i] > 0) {
      close(replicas[rep_index].voter_rep_in_copy[i]);
    }
  }

  return;
}

// Steal the buffers from a single replica
// buff_count and buffer should already be alocated.
void stealBuffers(int rep_num, char **buffer, int *buff_count) {
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
        buff_count[i] = read(replicas[rep_num].voter_rep_in_copy[i], buffer[i], MAX_PIPE_BUFF);
        if (buff_count[i] < 0) {
          perror("Voter error stealing pipe");
        }
      }
    }
  }  
}

// replace buffers in a replica. Does NOT free the buffer (so the same one can be copied again)
void returnBuffers(int rep_num, char **buffer, int *buff_count) {  
  int i = 0;
  for (i = 0; i < replicas[rep_num].pipe_count; i++) {
    if (buff_count[i] > 0) {
      writeBuffer(replicas[rep_num].vot_pipes[i].fd_out, buffer[i], buff_count[i]);
    }
  }
}

void restartReplica(int restarter, int restartee) {
  int i, retval;

  #ifdef TIME_RESTART_REPLICA
    timestamp_t last = generate_timestamp();
  #endif

  // reset timer
  if (timer_started) {
    // reset the timer
    timer_started = false;
  }

  cleanupReplica(restartee);

  //#ifdef TIME_RESTART_SIGNAL
  //  timestamp_t curr_time = generate_timestamp();
  //  union sigval time_value;
  //  time_value.sival_ptr = (void *)curr_time;
  //  int retval = sigqueue(replicas[restarter].pid, RESTART_SIGNAL, time_value);
  //#else

  // Make the restarter the most special of all the replicas
  for (i = 0; i < rep_count; i++) {    
    int dontcare = 0;
    int offset;
    if (i != restartee) {
      if (i == restarter) {
        offset = voter_priority - 2 + VOTER_PRIO_OFFSET;
      } else {
        offset = voter_priority + VOTER_PRIO_OFFSET;
      }
      retval = sched_set_realtime_policy(replicas[i].pid, &dontcare, offset);
      if (retval == 0) {
        // Do nothing, worked fine.
      } else {
        printf("Voter error call sched_set_realtime_policy in restartReplica for %s, priority %d, retval: %d\n", controller_name, offset, retval);
      }
    }
  }

  retval = kill(replicas[restarter].pid, RESTART_SIGNAL);
  //#endif /* TIME_RESTART_SIGNAL */
  if (retval < 0) {
    perror("VoterD Signal Problem");
  }

  // re-init failed rep, create pipes
  initReplicas(&(replicas[restartee]), 1, controller_name, voter_priority + VOTER_PRIO_OFFSET);
  createPipes(&(replicas[restartee]), 1, ext_pipes, pipe_count);
  // send new pipe through fd server (should have a request)
  acceptSendFDS(&sd, &(replicas[restartee].pid), replicas[restartee].rep_pipes, replicas[restartee].pipe_count);

  for (i = 0; i < replicas[restarter].pipe_count; i++) {
    if (replicas[restarter].vot_pipes[i].fd_in != 0) { // && replicas[restarter].voted[i] > 0) { // was causing problems with the timer
      replicas[restartee].voted[i] = replicas[restarter].voted[i];
      memcpy(replicas[restartee].vot_pipes[i].buffer, replicas[restarter].vot_pipes[i].buffer, replicas[restarter].vot_pipes[i].buff_count);
      replicas[restartee].vot_pipes[i].buff_count = replicas[restarter].vot_pipes[i].buff_count;
      sendPipe(i, restarter);
    }
  }

  #ifdef TIME_RESTART_REPLICA
    timestamp_t current = generate_timestamp();
    printf("(%lld)\n", current - last);
  #endif
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterD(void) {
  struct sigevent sev;
  sigset_t mask;

  InitTAS(DEFAULT_CPU, voter_priority);

  // Setup fd server
  createFDS(&sd, controller_name);

  initVotingState();
 
  startReplicas();
  
  return 0;
}

int parseArgs(int argc, const char **argv) {
  int i;
  int required_args = 5; // voter name, controller name, rep_type, timeout and priority
  controller_name = (char*) (argv[1]);
  rep_type = reptypeToEnum((char*)(argv[2]));
  rep_count = rep_type;
  voting_timeout = atoi(argv[3]);
  voter_priority = atoi(argv[4]);
  if (voting_timeout == 0) {
    voting_timeout = PERIOD_NSEC;
  }

  if (argc <= required_args) { // In testing mode // TODO: clear out after testing
    pid_t currentPID = getpid();
    //pipe_count = 4;  // 4 is the only controller specific bit here... and ArtPotTest
    //connectRecvFDS(currentPID, ext_pipes, pipe_count, "ArtPotTest");
    pipe_count = 2;  // 4 is the only controller specific bit here... and ArtPotTest
    connectRecvFDS(currentPID, ext_pipes, pipe_count, "EmptyTest");
    timer_start_index = 0;
    timer_stop_index = 1;
        // puts("Usage: VoterD <controller_name> <rep_type> <timeout> <priority> <message_type:fd_in:fd_out> <...>");
    // return -1;
  } else {
    for (i = 0; (i < argc - required_args && i < PIPE_LIMIT); i++) {
      deserializePipe(argv[i + required_args], &ext_pipes[pipe_count]);
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

  while(1) {
    doOneUpdate();
  }

  return 0;
}

void doOneUpdate(void) {
  int p_index, r_index;
  int retval = 0;

  struct timeval select_timeout;
  fd_set select_set;

  waitpid(-1, NULL, WNOHANG); // Seems to take a while for to clean up zombies

  // See if any of the read pipes have anything
  select_timeout.tv_sec = 1;
  select_timeout.tv_usec = 0;

  FD_ZERO(&select_set);
  // Check external in pipes
  for (p_index = 0; p_index < pipe_count; p_index++) {
    if (ext_pipes[p_index].fd_in != 0) {
      int e_pipe_fd = ext_pipes[p_index].fd_in;
      FD_SET(e_pipe_fd, &select_set);
    }
  }
  // Check pipes from replicas
  for (r_index = 0; r_index < rep_count; r_index++) {
    for (p_index = 0; p_index < replicas[r_index].pipe_count; p_index++) {
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
          ext_pipes[p_index].buff_count = TEMP_FAILURE_RETRY(read(read_fd, ext_pipes[p_index].buffer, MAX_PIPE_BUFF));
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
    for (r_index = 0; r_index < rep_count; r_index++) {
      for (p_index = 0; p_index < replicas[r_index].pipe_count; p_index++) {
        struct typed_pipe* curr_pipe = &(replicas[r_index].vot_pipes[p_index]);
        if (curr_pipe->fd_in !=0) {
          if (FD_ISSET(curr_pipe->fd_in, &select_set)) {
            processFromRep(r_index, p_index);
          }
        }
      }
    }
  }
}

void writeBuffer(int fd_out, char* buffer, int buff_count) {
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

int rep_gap(int rep_num) {
  int p_index = 0;
  int gap = 0;
  for (p_index = 0; p_index < pipe_count; p_index++) {
    if (replicas[0].vot_pipes[p_index].fd_in != 0) { // out from the rep, in to the voter
      gap += replicas[aheadRep(p_index)].voted[p_index] - replicas[rep_num].voted[p_index];
    }
  }
  return gap;
}

void balanceReps(void) {
  int starting = 0; // most behind rep gets data first
  int second = 1; // the most behind might be dead, so second to go is up next
  int index = 0;


  for (index = 0; index < rep_count; index++) {
    if (rep_gap(index) > rep_gap(starting)) {
      starting = index;
    } else if (rep_gap(index) > rep_gap(second)) {
      if (index != starting) {
        second = index;
      }
    }
  }

  for (index = 0; index < rep_count; index++) {    
    int dontcare = 0;
    int offset;
    if (index == starting) {
      offset = voter_priority - 2 + VOTER_PRIO_OFFSET;
    } else if (index == second) {
      offset = voter_priority - 1 + VOTER_PRIO_OFFSET;
    } else {
      offset = voter_priority + VOTER_PRIO_OFFSET;
    }
    int retval = sched_set_realtime_policy(replicas[index].pid, &dontcare, offset);
    if (retval == 0) {
      // Do nothing, worked fine.
    } else if (retval == SCHED_ERROR_NOEXIST) {
      // Likely means that the process as died. Will restart through a timeout.
    } else {
      printf("Voter error call sched_set_realtime_policy for %s, priority %d, retval: %d\n", controller_name, offset, retval);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process data
void processData(struct typed_pipe *pipe, int pipe_index) {
  int r_index;
  if (pipe_index == timer_start_index) {
    if (!timer_started) {
      timer_started = true;
      watchdog = generate_timestamp();
    }
  }

  balanceReps();

  for (r_index = 0; r_index < rep_count; r_index++) {
    writeBuffer(replicas[r_index].vot_pipes[pipe_index].fd_out, pipe->buffer, pipe->buff_count);
  }
}

////////////////////////////////////////////////////////////////////////////////
// reset / init voting state
void resetVotingState(int pipe_num) {
  int r_index;
  for (r_index = 0; r_index < rep_count; r_index++) {
    replicas[r_index].voted[pipe_num]--; // = 0;
  }
}

void initVotingState(void) {
  int p_index;
  int r_index;
  for (p_index = 0; p_index < pipe_count; p_index++) {
    for (r_index = 0; r_index < rep_count; r_index++) {
      replicas[r_index].voted[p_index] = 0;
    }
  }
}

bool allReporting(int pipe_num) {
  bool all_reporting = true;
  int r_index;
  for (r_index = 0; r_index < rep_count; r_index++) {
    // Check that all have reported
    all_reporting = all_reporting && 
      (replicas[r_index].voted[pipe_num] == replicas[(r_index + 1) % rep_count].voted[pipe_num]);
  }

  return all_reporting;
}

void sendPipe(int pipe_num, int replica_num) {
  if (!allReporting(pipe_num)) {
    return;
  }
  
  if (pipe_num == timer_stop_index) {  
    // reset the timer
    timer_started = false;
  }

  writeBuffer(ext_pipes[pipe_num].fd_out, replicas[replica_num].vot_pipes[pipe_num].buffer, replicas[replica_num].vot_pipes[pipe_num].buff_count);
  
  resetVotingState(pipe_num);
}

void checkSDC(int pipe_num) {
  int r_index;

  if (!allReporting(pipe_num)) {
    return;
  }

  switch (rep_type) {
    case SMR: 
      // Only one rep, so pretty much have to trust it
      sendPipe(pipe_num, 0);
      return;
    case DMR:
      // Can detect, and check what to do
      if (memcmp(replicas[0].vot_pipes[pipe_num].buffer,
                 replicas[1].vot_pipes[pipe_num].buffer,
                 replicas[0].vot_pipes[pipe_num].buff_count) != 0) {
        printf("Voting disagreement: caught SDC in DMR but can't do anything about it.\n");
      }

      sendPipe(pipe_num, 0);
      return;
    case TMR:
      // Send the solution that at least two agree on
      // TODO: What if buff_count is off?
      for (r_index = 0; r_index < rep_count; r_index++) {
        if (memcmp(replicas[r_index].vot_pipes[pipe_num].buffer,
                   replicas[(r_index+1) % rep_count].vot_pipes[pipe_num].buffer,
                   replicas[r_index].vot_pipes[pipe_num].buff_count) == 0) {

          // If the third doesn't agree, it should be restarted.
          if (memcmp(replicas[r_index].vot_pipes[pipe_num].buffer,
                     replicas[(r_index + 2) % rep_count].vot_pipes[pipe_num].buffer,
                     replicas[r_index].vot_pipes[pipe_num].buff_count) != 0) {
            int restartee = (r_index + 2) % rep_count;
            int other_rep = (r_index + 1) % rep_count;
            int restarter = r_index;
            printf("Voting disagreement: caught SDC Name %s\t Rep %d\t Pipe %d\t pid %d\n", controller_name, restartee, pipe_num, replicas[restartee].pid);

            int i;
            char **restarter_buffer = (char **)malloc(sizeof(char *) * PIPE_LIMIT);
            if (restarter_buffer == NULL) {
              perror("Voter failed to malloc memory");
            }
            char **other_rep_buffer = (char **)malloc(sizeof(char *) * PIPE_LIMIT);
            if (other_rep_buffer == NULL) {
              perror("Voter failed to malloc memory");
            }
            for (i = 0; i < PIPE_LIMIT; i++) {
              restarter_buffer[i] = (char *)malloc(sizeof(char) * MAX_PIPE_BUFF);
              if (restarter_buffer[i] == NULL) {
                perror("Voter failed to allocat memory");
              }
              other_rep_buffer[i] = (char *)malloc(sizeof(char) * MAX_PIPE_BUFF);
              if (other_rep_buffer[i] == NULL) {
                perror("Voter failed to allocat memory");
              }
            }
            int restarter_buff_count[PIPE_LIMIT] = {0};
            int other_rep_buff_count[PIPE_LIMIT] = {0};

            // Steal the buffers from healthy reps. This stops them from processing mid restart
            stealBuffers(restarter, restarter_buffer, restarter_buff_count);
            stealBuffers(other_rep, other_rep_buffer, other_rep_buff_count);

            restartReplica(restarter, restartee);

            // Give the buffers back
            returnBuffers(restartee, restarter_buffer, restarter_buff_count);
            returnBuffers(restarter, restarter_buffer, restarter_buff_count);
            returnBuffers(other_rep, other_rep_buffer, other_rep_buff_count);
            // free the buffers
            for (i = 0; i < PIPE_LIMIT; i++) {
              free(restarter_buffer[i]);
              free(other_rep_buffer[i]);
            }
            free(restarter_buffer);
            free(other_rep_buffer);
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

// New message came in, but already have a message
void emergencyWrite(int pipe_num, int replica_num) {
  switch (rep_type) {
  case SMR: 
    // Wut? This makes no sense
    printf("Voter emergencyWrite error: SMR.");
    return;
  case DMR:
    // Just send it.
    writeBuffer(ext_pipes[pipe_num].fd_out, replicas[replica_num].vot_pipes[pipe_num].buffer, replicas[replica_num].vot_pipes[pipe_num].buff_count);
    resetVotingState(pipe_num);
    return;
  case TMR:
    // check if match? or just send it.
    writeBuffer(ext_pipes[pipe_num].fd_out, replicas[replica_num].vot_pipes[pipe_num].buffer, replicas[replica_num].vot_pipes[pipe_num].buff_count);
    resetVotingState(pipe_num);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process output from replica; vote on it
void processFromRep(int replica_num, int pipe_num) {
  if (replicas[replica_num].voted[pipe_num] >= 1) { // Already have a pending message... will lose data if read is done
    emergencyWrite(pipe_num, replica_num);
  }
  struct typed_pipe* curr_pipe = &(replicas[replica_num].vot_pipes[pipe_num]);
  curr_pipe->buff_count = TEMP_FAILURE_RETRY(read(curr_pipe->fd_in, curr_pipe->buffer, MAX_PIPE_BUFF));

  // TODO: Read may have been interrupted
  if (curr_pipe->buff_count > 0) {
    replicas[replica_num].voted[pipe_num]++;

    balanceReps();
    
    if (replicas[replica_num].voted[pipe_num] > 1) {
      // Happens when a process has died.
      // printf("Run-away lag detected: %s pipe - %d - rep 0, 1, 2: %d, %d, %d\n", controller_name, pipe_num, replicas[0].voted[pipe_num], replicas[1].voted[pipe_num], replicas[2].voted[pipe_num]);
    }

    checkSDC(pipe_num);
  } else if (curr_pipe->buff_count < 0) {
    printf("Voter - Controller %s, rep %d, pipe %d\n", controller_name, replica_num, pipe_num);
    perror("Voter - read problem on internal pipe");
  } else {
    printf("Voter - Controller %s, rep %d, pipe %d\n", controller_name, replica_num, pipe_num);
    perror("Voter - read == 0 on internal pipe");
  }

  if (timer_started) {
    timestamp_t current = generate_timestamp();
    if (((current - watchdog) / 3.092) > voting_timeout) {
      printf("Restarting Rep. due to timeout. Name %s\n", controller_name);
      voterRestartHandler();
    }
  }
}
