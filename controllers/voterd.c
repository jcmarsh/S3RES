/*
 * Voter that is able to start and connect to other voters, and maybe
 * even be generic
 *
 * Author - James Marshall
 */

#include <assert.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#include "../include/controller.h"
#include "../include/replicas.h"
#include "../include/fd_server.h"

#define REP_MAX 3
#define PERIOD_NSEC 120000 // Max time for voting in nanoseconds (120 micro seconds)

long voting_timeout;
int timer_start_index;
int timer_stop_index;
bool timer_started;

// Replica related data
struct replica replicas[REP_MAX];

// TAS Stuff
cpu_speed_t cpu_speed;
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
int initVoterD();
int parseArgs(int argc, const char **argv);
int main(int argc, const char **argv);
void doOneUpdate();
void processData(struct typed_pipe pipe, int pipe_index);
void resetVotingState();
void checkSend(int pipe_num, bool checkSDC);
void processFromRep(int replica_num, int pipe_num);
void restartReplica(int restarter, int restartee);
void cleanupReplica(int rep_index);

void timeout_sighandler(int signo, siginfo_t *si, void *unused) {
  assert(write(timeout_fd[1], timeout_byte, 1) == 1);
}

void startReplicas(void) {
  initReplicas(replicas, rep_count, controller_name, voter_priority + 5);
  createPipes(replicas, rep_count, ext_pipes, pipe_count);
  forkReplicas(replicas, rep_count);
  for (int i = 0; i < rep_count; i++) {
    if (acceptSendFDS(&sd, &(replicas[i].pid), replicas[i].rep_pipes, replicas[i].pipe_count) < 0) {
      printf("VoterD acceptSendFDS call failed\n");
      exit(-1);
    }
  }
}

void restartHandler() {
  // Timer went off, so the timer_stop_index is the pipe which is awaiting a rep

  switch (rep_type) {
    case SMR:
      // Need to cold restart the replica
      cleanupReplica(0);

      startReplicas();
      
      // Resend last data
      for (int p_index = 0; p_index < pipe_count; p_index++) {
        int read_fd = ext_pipes[p_index].fd_in;
        if (read_fd != 0) {
          processData(ext_pipes[p_index], p_index);    
        }
      }

      return;
    case DMR:
      // Same as TMR
    case TMR:
      for (int r_index = 0; r_index < rep_count; r_index++) {
        if (replicas[r_index].voted[timer_stop_index] == false) {

          // This is the failed replica, restart it
          // Send a signal to the rep's friend
          int restarter = (r_index + (rep_count - 1)) % rep_count;
          int restartee = r_index;

          #ifdef TIME_RESTART_REPLICA
            timestamp_t last = generate_timestamp();
          #endif
          restartReplica(restarter, restartee);
          #ifdef TIME_RESTART_REPLICA
            timestamp_t current = generate_timestamp();
            printf("(%lld)\n", current - last);
          #endif

          // Send along the response from the other two replicas.
          // also copy over the previous vote state and pipe buffers
          for (int i = 0; i < replicas[restarter].pipe_count; i++) {
            replicas[restartee].voted[i] = replicas[restarter].voted[i];
            memcpy(replicas[restartee].vot_pipes[i].buffer, replicas[restarter].vot_pipes[i].buffer, replicas[restarter].vot_pipes[i].buff_count);
            replicas[restartee].vot_pipes[i].buff_count = replicas[restarter].vot_pipes[i].buff_count;
            checkSend(i, false); // DO NOT check for SDC (one has failed)
          }
          return;
        }
      }
  }
}

void cleanupReplica(int rep_index) {
  // Kill old replica
  kill(replicas[rep_index].pid, SIGKILL); // Make sure it is dead.
  waitpid(-1, NULL, WNOHANG); // cleans up the zombie // Actually doesn't // Well, now it does.

  
  // cleanup replica data structure
  for (int i = 0; i < replicas[rep_index].pipe_count; i++) {
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
  initReplicas(&(replicas[restartee]), 1, controller_name, voter_priority + 5);
  createPipes(&(replicas[restartee]), 1, ext_pipes, pipe_count);
  // send new pipe through fd server (should have a request)

  acceptSendFDS(&sd, &(replicas[restartee].pid), replicas[restartee].rep_pipes, replicas[restartee].pipe_count);
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterD() {
  struct sigevent sev;
  sigset_t mask;

  InitTAS(DEFAULT_CPU, &cpu_speed, voter_priority);

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

  resetVotingState();
 
  startReplicas();
  
  return 0;
}

int parseArgs(int argc, const char **argv) {
  int required_args = 5; // voter name, controller name, rep_type, timeout and priority
  controller_name = const_cast<char*>(argv[1]);
  rep_type = reptypeToEnum(const_cast<char*>(argv[2]));
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
    for (int i = 0; (i < argc - required_args && i < PIPE_LIMIT); i++) {
      deserializePipe(argv[i + required_args], &ext_pipes[pipe_count]);
      pipe_count++;
    }
    if (pipe_count >= PIPE_LIMIT) {
      printf("VoterD: Raise pipe limit.\n");
    }
  
    for (int i = 0; i < pipe_count; i++) {
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

void doOneUpdate() {
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
  for (int p_index = 0; p_index < pipe_count; p_index++) {
    if (ext_pipes[p_index].fd_in != 0) {
      int e_pipe_fd = ext_pipes[p_index].fd_in;
      FD_SET(e_pipe_fd, &select_set);
    }
  }
  // Check pipes from replicas
  for (int r_index = 0; r_index < rep_count; r_index++) {
    for (int p_index = 0; p_index < replicas[r_index].pipe_count; p_index++) {
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
      retval = read(timeout_fd[0], timeout_byte, 1);
      if (retval > 0) {
        //printf("Restarting Rep.\n");
        restartHandler();
      } else {
        // TODO: Do I care about this?
      }
    }
    
    // Check for data from external sources
    for (int p_index = 0; p_index < pipe_count; p_index++) {
      int read_fd = ext_pipes[p_index].fd_in;
      if (read_fd != 0) {
        if (FD_ISSET(read_fd, &select_set)) {
          ext_pipes[p_index].buff_count = read(read_fd, ext_pipes[p_index].buffer, MAX_PIPE_BUFF);
          processData(ext_pipes[p_index], p_index);
        }
      }
    }

    // Check all replicas for data
    for (int r_index = 0; r_index < rep_count; r_index++) {
      for (int p_index = 0; p_index < replicas[r_index].pipe_count; p_index++) {
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

////////////////////////////////////////////////////////////////////////////////
// Process data
void processData(struct typed_pipe pipe, int pipe_index) {
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

  for (int r_index = 0; r_index < rep_count; r_index++) {
    int written = write(replicas[r_index].vot_pipes[pipe_index].fd_out, pipe.buffer, pipe.buff_count);
    if (written != pipe.buff_count) {
      printf("VoterD: bytes written: %d\texpected: %d\n", written, pipe.buff_count);
      perror("VoterD failed write to replica\n");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// reset / init voting state
void resetVotingState(int pipe_num) {
  for (int r_index = 0; r_index < rep_count; r_index++) {
    replicas[r_index].voted[pipe_num] = false;
  }
}

void resetVotingState() {
  for (int p_index = 0; p_index < pipe_count; p_index++) {
    resetVotingState(p_index);
  }
}

void checkSend(int pipe_num, bool checkSDC) {
  bool all_reporting = true;
  for (int r_index = 0; r_index < rep_count; r_index++) {
    // Check that all have reported
    all_reporting = all_reporting && replicas[r_index].voted[pipe_num];
  }

  if (!all_reporting) {
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

  int retval;
  switch (rep_type) {
    case SMR: 
      // Only one rep, so pretty much have to trust it
      retval = write(ext_pipes[pipe_num].fd_out, replicas[0].vot_pipes[pipe_num].buffer,
                        replicas[0].vot_pipes[pipe_num].buff_count);
      if (retval == 0) {
        perror("Seriously Voter?");
      }
      
      resetVotingState(pipe_num);
      return;
    case DMR:
      // Can detect, and check what to do
      retval = write(ext_pipes[pipe_num].fd_out, replicas[0].vot_pipes[pipe_num].buffer,
                        replicas[0].vot_pipes[pipe_num].buff_count);
      if (retval == 0) {
        perror("Seriously Voter?");
      }

      if (checkSDC) {
        if (memcmp(replicas[0].vot_pipes[pipe_num].buffer,
                   replicas[1].vot_pipes[pipe_num].buffer,
                   replicas[0].vot_pipes[pipe_num].buff_count) != 0) {
          printf("Voting disagreement: caught SDC in DMR but can't do anything about it.\n");

          // Can't restart so don't know which is wrong.
          // restartReplica(r_index, (r_index + 2) % rep_count);
        }
      }
      
      resetVotingState(pipe_num);
      return;
    case TMR:
      // Send the solution that at least two agree on
      // TODO: What if buff_count is off?
      for (int r_index = 0; r_index < rep_count; r_index++) {
        if (memcmp(replicas[r_index].vot_pipes[pipe_num].buffer,
                   replicas[(r_index+1) % rep_count].vot_pipes[pipe_num].buffer,
                   replicas[r_index].vot_pipes[pipe_num].buff_count) == 0) {

          if (checkSDC) {
            // If the third doesn't agree, it should be restarted.
            if (memcmp(replicas[r_index].vot_pipes[pipe_num].buffer,
                       replicas[(r_index + 2) % rep_count].vot_pipes[pipe_num].buffer,
                       replicas[r_index].vot_pipes[pipe_num].buff_count) != 0) {
              //printf("Voting disagreement: caught SDC on pipe: %d\n", pipe_num);

              int restartee = (r_index + 2) % rep_count;
              restartReplica(r_index, restartee);

              // TODO: This is similar to code in the restart timeout handler
              for (int i = 0; i < replicas[r_index].pipe_count; i++) {
                replicas[restartee].voted[i] = replicas[r_index].voted[i];
                memcpy(replicas[restartee].vot_pipes[i].buffer, replicas[r_index].vot_pipes[i].buffer, replicas[r_index].vot_pipes[i].buff_count);
                replicas[restartee].vot_pipes[i].buff_count = replicas[r_index].vot_pipes[i].buff_count;
                //checkSend(i, false); // DO NOT check for SDC (one has failed)
              }
            }
          }
          resetVotingState(pipe_num);

          retval = write(ext_pipes[pipe_num].fd_out, replicas[r_index].vot_pipes[pipe_num].buffer,
                replicas[r_index].vot_pipes[pipe_num].buff_count);
          if (retval == 0) {
            perror("Seriously Voter?");
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
  curr_pipe->buff_count = read(curr_pipe->fd_in, curr_pipe->buffer, MAX_PIPE_BUFF);
  if (curr_pipe->buff_count <= 0) {
    perror("Failed to copy buffer\n");
  }

  if (replicas[replica_num].voted[pipe_num] == true) {
    // This happens when one of the reps has failed, but the watchdog has not expired yet. Other reps are responding to the next round of data.
    // The old data is simply dropped, since new data is arriving. A solution sending the old result could be made, but a second set of buffers would be needed.
    // TODO: It would be BAD if a rep sent a second response by accident.... this needs to be more robust.
    //printf("ERROR: Replica already voted. Name %s\t Rep %d\t Pipe %d\n", controller_name, replica_num, pipe_num);
    resetVotingState(pipe_num);
  }
  replicas[replica_num].voted[pipe_num] = true;

  checkSend(pipe_num, true);
}
