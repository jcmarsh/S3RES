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
#include <time.h>

#include "../include/replicas.h"
#include "../include/fd_server.h"
#include "../include/scheduler.h"

#define REP_MAX 3
#define PERIOD_NSEC 120000 // Max time for voting in nanoseconds (120 micro seconds)
#define VOTER_PRIO_OFFSET 20 // Replicas run with a +20 offset (low prio)

long voting_timeout;
int timer_start_index;
int timer_stop_index;
bool timer_started;

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

// restart timer fd
char timeout_byte[1] = {'*'};
int timeout_fd[2];
timer_t timerid;
struct itimerspec its;

// Functions!
int initVoterD(void);
int parseArgs(int argc, const char **argv);
int main(int argc, const char **argv);
void doOneUpdate(void);
void processData(struct typed_pipe pipe, int pipe_index);
void resetVotingState(int pipe_num);
void resetVotingStateAll(void);
void sendPipe(int pipe_num, int replica_num);
void checkSDC(int pipe_num);
void processFromRep(int replica_num, int pipe_num);
void restartReplica(int restarter, int restartee);
void cleanupReplica(int rep_index);

void timeout_sighandler(int signo, siginfo_t *si, void *unused) {
  assert(TEMP_FAILURE_RETRY(write(timeout_fd[1], timeout_byte, 1) == 1));
}

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
  int p_index, r_index, i;

  switch (rep_type) {
    case SMR:
      // Need to cold restart the replica
      cleanupReplica(0);

      startReplicas();
      
      // Resend last data
      for (p_index = 0; p_index < pipe_count; p_index++) {
        int read_fd = ext_pipes[p_index].fd_in;
        if (read_fd != 0) {
          processData(ext_pipes[p_index], p_index);    
        }
      }

      return;
    case DMR:
      // Same as TMR
    case TMR: ; // the semicolon is needed becasue C.
      // The failed rep should be the one behind on the timer pipe
      int restartee = behindRep(timer_stop_index);
      int restarter = (restartee + (rep_count - 1)) % rep_count;

      restartReplica(restarter, restartee);
      return;
  }
}

void cleanupReplica(int rep_index) {
  int i;
  // Kill old replica
  kill(replicas[rep_index].pid, SIGKILL); // Make sure it is dead.
  waitpid(-1, NULL, WNOHANG); // cleans up the zombie // Actually doesn't // Well, now it does.

  
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
  }

  return;
}

void restartReplica(int restarter, int restartee) {
  // Send along the response from the other two replicas.
  // also copy over the previous vote state and pipe buffers
  int i;
  for (i = 0; i < replicas[restarter].pipe_count; i++) {
    replicas[restartee].voted[i] = replicas[restarter].voted[i];
    memcpy(replicas[restartee].vot_pipes[i].buffer, replicas[restarter].vot_pipes[i].buffer, replicas[restarter].vot_pipes[i].buff_count);
    replicas[restartee].vot_pipes[i].buff_count = replicas[restarter].vot_pipes[i].buff_count;
    sendPipe(i, restarter);
  }

  #ifdef TIME_RESTART_REPLICA
    timestamp_t last = generate_timestamp();
  #endif
  cleanupReplica(restartee);

  #ifdef TIME_RESTART_SIGNAL
    timestamp_t curr_time = generate_timestamp();
    union sigval time_value;
    time_value.sival_ptr = (void *)curr_time;
    int retval = sigqueue(replicas[restarter].pid, RESTART_SIGNAL, time_value);
  #else
    int retval = kill(replicas[restarter].pid, RESTART_SIGNAL);
  #endif /* TIME_RESTART_SIGNAL */
  if (retval < 0) {
    perror("VoterD Signal Problem");
  }

  // re-init failed rep, create pipes
  initReplicas(&(replicas[restartee]), 1, controller_name, voter_priority + VOTER_PRIO_OFFSET);
  createPipes(&(replicas[restartee]), 1, ext_pipes, pipe_count);
  // send new pipe through fd server (should have a request)

  acceptSendFDS(&sd, &(replicas[restartee].pid), replicas[restartee].rep_pipes, replicas[restartee].pipe_count);
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

  // timeout_fd
  if (pipe(timeout_fd) == -1) {
    perror("VoterD time out pipe fail");
    return -1;
  }

  // create timer handler
  struct sigaction sa;

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = timeout_sighandler;
  if (sigaction(TIMEOUT_SIGNAL, &sa, NULL) == -1) {
    perror("Voter failed to register the watchdog handler");
    return -1;
  }

  /* Create the timer */
  memset(&sev, 0, sizeof(sev));
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = TIMEOUT_SIGNAL;
  sev.sigev_value.sival_ptr = &timerid;
  if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
    perror("VoterD timer_create failed");
    return -1;
  }

  // Setup fd server
  createFDS(&sd, controller_name);

  resetVotingStateAll();
 
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

  // See if any of the read pipes have anything
  select_timeout.tv_sec = 1;
  select_timeout.tv_usec = 0;

  FD_ZERO(&select_set);
  // Check for timeouts
  FD_SET(timeout_fd[0], &select_set);
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
    // Check for failed replica (time out)
    if (FD_ISSET(timeout_fd[0], &select_set)) {
      retval = TEMP_FAILURE_RETRY(read(timeout_fd[0], timeout_byte, 1));
      if (retval > 0) { // Only one byte, so I can't imagine how that could be interrupted.
        printf("Restarting Rep. due to timeout. Name %s\n", controller_name);
        voterRestartHandler();
      } else {
        printf("Voter - Controller %s\n", controller_name);
        perror("Voter - read error on timeout pipe");
      }
    }
    
    // Check for data from external sources
    for (p_index = 0; p_index < pipe_count; p_index++) {
      int read_fd = ext_pipes[p_index].fd_in;
      if (read_fd != 0) {
        if (FD_ISSET(read_fd, &select_set)) {
          ext_pipes[p_index].buff_count = TEMP_FAILURE_RETRY(read(read_fd, ext_pipes[p_index].buffer, MAX_PIPE_BUFF));
          if (ext_pipes[p_index].buff_count > 0) { // TODO: read may still have been interrupted
            processData(ext_pipes[p_index], p_index);
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

void writeBuffer(int fd_out, char** buffer, int buff_count) {
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

void balanceReps(void) {
  int starting = 0; //behindRep(pipe_index + 2); // most behind rep gets data first
  int gap = 0;
  int index = 0;
  for (index = 0; index < pipe_count; index++) {
    if (replicas[0].vot_pipes[index].fd_in != 0) { // out from the rep, in to the voter
      if ((replicas[aheadRep(index)].voted[index] - 0) > gap) { //replicas[behindRep(i)].voted[i]) > gap) {
        gap = replicas[aheadRep(index)].voted[index] - 0; //replicas[behindRep(i)].voted[i];
        starting = behindRep(index);
      }
    }
  }

  for (index = 0; index < rep_count; index++) {    
    int dontcare = 0;
    int offset;
    if (index == starting) {
      offset = voter_priority - 1 + VOTER_PRIO_OFFSET;
    } else {
      offset = voter_priority + VOTER_PRIO_OFFSET;
    }
    int retval = sched_set_realtime_policy(replicas[index].pid, &dontcare, offset);
    if (retval == 0) {
      // Do nothing, worked fine.
    } else if (retval == SCHED_ERROR_NOEXIST) {
      printf("Voter restarting %s - %d, detected by scheduling failure\n", controller_name, index);
      int restarter = (index + (rep_count - 1)) % rep_count;
      restartReplica(restarter, index);
    } else {
      printf("Voter error call sched_set_realtime_policy for %s, priority %d, retval: %d\n", controller_name, offset, retval);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process data
void processData(struct typed_pipe pipe, int pipe_index) {
  int r_index;
  if (pipe_index == timer_start_index) {
    if (!timer_started) {
      timer_started = true;
      // Arm timer
      its.it_interval.tv_sec = 0;
      its.it_interval.tv_nsec = 0;
      its.it_value.tv_sec = 0;
      its.it_value.tv_nsec = voting_timeout;

      if (timer_settime(timerid, 0, &its, NULL) == -1) {
        perror("VoterD timer_settime failed");
      }
    }
  }

  balanceReps();

  for (r_index = 0; r_index < rep_count; r_index++) {
    writeBuffer(replicas[r_index].vot_pipes[pipe_index].fd_out, pipe.buffer, pipe.buff_count);
  }
}

////////////////////////////////////////////////////////////////////////////////
// reset / init voting state
void resetVotingState(int pipe_num) {
  int r_index;
  for (r_index = 0; r_index < rep_count; r_index++) {
    replicas[r_index].voted[pipe_num] = 0;
  }
}

void resetVotingStateAll(void) {
  int p_index;
  for (p_index = 0; p_index < pipe_count; p_index++) {
    resetVotingState(p_index);
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
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 0;

    if (timer_settime(timerid, 0, &its, NULL) == -1) {
      perror("VoterD timer_settime failed");
    }
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
            printf("Voting disagreement: caught SDC Name %s\t Rep %d\t Pipe %d\n", controller_name, restartee, pipe_num);

            restartReplica(r_index, restartee);
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
  struct typed_pipe* curr_pipe = &(replicas[replica_num].vot_pipes[pipe_num]);
  curr_pipe->buff_count = TEMP_FAILURE_RETRY(read(curr_pipe->fd_in, curr_pipe->buffer, MAX_PIPE_BUFF));
  // TODO: Read may have been interrupted
  if (curr_pipe->buff_count > 0) {
    replicas[replica_num].voted[pipe_num]++;
    balanceReps();

    if (replicas[replica_num].voted[pipe_num] > 1) {
      printf("Run-away lag detected: %s pipe - %d - rep 0, 1, 2: %d, %d, %d\n", controller_name, pipe_num, replicas[0].voted[pipe_num], replicas[1].voted[pipe_num], replicas[2].voted[pipe_num]);
    }

    checkSDC(pipe_num);
  } else if (curr_pipe->buff_count < 0) {
    printf("Voter - Controller %s, rep %d, pipe %d\n", controller_name, replica_num, pipe_num);
    perror("Voter - read problem on internal pipe");
  } else {
    printf("Voter - Controller %s, rep %d, pipe %d\n", controller_name, replica_num, pipe_num);
    perror("Voter - read == 0 on internal pipe");
  }
}
