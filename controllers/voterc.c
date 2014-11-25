/*
 * Voter that is able to start and connect to other voters, and maybe
 * even be generic
 *
 * Author - James Marshall
 */

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../include/taslimited.h"
#include "../include/statstime.h"
#include "../include/replicas.h"
#include "../include/commtypes.h"
#include "../include/fd_server.h"

#define SIG SIGRTMIN + 7
#define REP_COUNT 3
#define INIT_ROUNDS 4
#define PERIOD_NSEC 120000 // Max time for voting in nanoseconds (120 micro seconds)

// Either waiting for replicas to vote or waiting for the next round (next ranger input).
// Or a replica has failed and recovery is needed
typedef enum {
  VOTING,
  RECOVERY,
  WAITING
} voting_status;

int voting_timeout;

// Replica related data
struct replica replicas[REP_COUNT];

// TAS Stuff
cpu_speed_t cpu_speed;

// FD server
struct server_data sd;

char* controller_name;
// pipes to external components (not replicas)
int pipe_count = 0;
struct typed_pipe ext_pipes[PIPE_LIMIT];
voting_status vote_stat;

// restart timer fd
char timeout_byte[1] = {'*'};
int timeout_fd[2];
timer_t timerid;
struct itimerspec its;

// Functions!
int initVoterC();
int parseArgs(int argc, const char **argv);
int main(int argc, const char **argv);
void doOneUpdate();
void processData(struct typed_pipe pipe, int pipe_index);
void resetVotingState();
void checkSend(int pipe_num);
void processFromRep(int replica_num, int pipe_num);

void timeout_sighandler(int signum) {//, siginfo_t *si, void *data) {
  if (vote_stat == VOTING) {
    assert(write(timeout_fd[1], timeout_byte, 1) == 1);
  }
}

void restartReplica() {
  int restart_id;

  // no real way to do this well unless every pipe has its own timer.
  // Assuming that only one of the reps has failed.
  for (int p_index = 0; p_index < pipe_count; p_index++) {
    for (int r_index = 0; r_index < REP_COUNT; r_index++) {
      if (replicas[r_index].vot_pipes[p_index].type == MOV_CMD &&
          replicas[r_index].voted[p_index] == false) {
        int status;
        waitpid(replicas[r_index].pid, &status, WNOHANG); // cleans up the zombie

        // This is the failed replica, restart it
        // Send a signal to the rep's friend
        restart_id = (r_index + (REP_COUNT - 1)) % REP_COUNT; // Plus 2 is minus 1!
        //printf("Restarting through %d\n", replicas[restart_id].pid);
        int retval = kill(replicas[restart_id].pid, SIGUSR1);
        if (retval < 0) {
          perror("VoterC Signal Problem");
        }
            
        // re-init failed rep, create pipes
        initReplicas(&(replicas[r_index]), 1, controller_name);
        createPipes(&(replicas[r_index]), 1, ext_pipes, pipe_count);
        // send new pipe through fd server (should have a request)
        acceptSendFDS(&sd, &(replicas[r_index].pid), replicas[r_index].rep_pipes, replicas[r_index].pipe_count);

        // Should send along the response from the other two replicas.
        replicas[r_index].voted[p_index] = true;
        checkSend(p_index);
        return;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterC() {
  struct sigevent sev;
  struct sigaction sa;
  sigset_t mask;

  InitTAS(DEFAULT_CPU, &cpu_speed, 3);

  // timeout_fd
  if (pipe(timeout_fd) == -1) {
    perror("VoterC time out pipe fail");
    return -1;
  }

  // create timer
  /* Establish handler for timer signal */
  //  sa.sa_flags = SA_SIGINFO;
  //sa.sa_sigaction = timeout_sighandler;
  //sigemptyset(&sa.sa_mask);
  //if (sigaction(SIG, &sa, NULL) == -1) {
  if (signal(SIG, timeout_sighandler) == SIG_ERR) {
    perror("VoterC sigaction failed");
    return -1;
  }

  sigemptyset(&mask);
  sigaddset(&mask, SIG);
  if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
    perror("VoterC sigprockmask failed");
    return -1;
  }

  /* Create the timer */
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIG;
  sev.sigev_value.sival_ptr = &timerid;
  if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
    perror("VoterC timer_create failed");
    return -1;
  }

  // Setup fd server
  createFDS(&sd, controller_name);

  // Let's try to launch the replicas
  initReplicas(replicas, REP_COUNT, controller_name);
  createPipes(replicas, REP_COUNT, ext_pipes, pipe_count);
  forkReplicas(replicas, REP_COUNT);
  
  resetVotingState();

  printf("VoterC replicas all launched\n");

  return 0;
}

int parseArgs(int argc, const char **argv) {
  if (argc < 4) {
    puts("Usage: VoterC <controller_name> <timeout> <message_type:fd_in:fd_out> <...>");
    return -1;
  }

  controller_name = const_cast<char*>(argv[1]);
  voting_timeout = atoi(argv[2]);
  if (voting_timeout == 0) {
    voting_timeout = PERIOD_NSEC;
  }
  for (int i = 0; (i < argc - 3 && i < PIPE_LIMIT); i++) {
    deserializePipe(argv[i + 3], &ext_pipes[pipe_count]);
    pipe_count++;
  }
  if (pipe_count >= PIPE_LIMIT) {
    printf("VoterC: Raise pipe limie.\n");
  }

  return 0;
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initVoterC() < 0) {
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
  for (int r_index = 0; r_index < REP_COUNT; r_index++) {
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
        printf("Restarting Rep.\n");
        restartReplica();
      } else {
        // TODO: Do I care about this?
      }
    }
    
    // Check for data from external sources
    for (int p_index = 0; p_index < pipe_count; p_index++) {
      int read_fd = ext_pipes[p_index].fd_in;
      if (read_fd != 0) {
        if (FD_ISSET(read_fd, &select_set)) {
          ext_pipes[p_index].buff_count = read(read_fd, ext_pipes[p_index].buffer, 1024); // TODO remove magic number
          processData(ext_pipes[p_index], p_index);
        }
      }
    }

    // Check all replicas for data
    for (int r_index = 0; r_index < REP_COUNT; r_index++) {
      for (int p_index = 0; p_index < replicas[r_index].pipe_count; p_index++) {
        struct typed_pipe* curr_pipe = &(replicas[r_index].vot_pipes[p_index]);
        if (curr_pipe->fd_in !=0) {
          if (FD_ISSET(curr_pipe->fd_in, &select_set)) {
            curr_pipe->buff_count = read(curr_pipe->fd_in, curr_pipe->buffer, 1024);
            if (curr_pipe->buff_count > 0) {
              processFromRep(r_index, p_index);  
            }
          }
        }
      }
    }
  } else if (errno == EINTR) {
    // Timer likely expired. Will loop back so no worries.
  } else {
    perror("VoterC select in main control loop");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process data
// TODO: This isn't really meant to handle multiple inputs yet
// Voting is only done on incoming RANGE_POSE_DATA
void processData(struct typed_pipe pipe, int pipe_index) {
  if (pipe.type == RANGE_POSE_DATA) {
    if (vote_stat == RECOVERY) { // Bad things: a replica is still recovering!
      printf("VoterC: New data while in recovery mode.");
    }

    // TODO Voting should be per-pipe
    vote_stat = VOTING;

    // Arm timer
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = voting_timeout;

    if (timer_settime(timerid, 0, &its, NULL) == -1) {
      perror("VoterC timer_settime failed");
    }
  }

  for (int r_index = 0; r_index < REP_COUNT; r_index++) {
    // TODO: this just matches direction and type
    for (int p_index = 0; p_index < replicas[r_index].pipe_count; p_index++) {
      if (replicas[r_index].vot_pipes[p_index].fd_out != 0 &&
          replicas[r_index].vot_pipes[p_index].type == pipe.type) {
        int written = write(replicas[r_index].vot_pipes[p_index].fd_out, pipe.buffer, pipe.buff_count);
        if (written != pipe.buff_count) {
          printf("VoterC: bytes written: %d\texpected: %d\n", written, pipe.buff_count);
          perror("VoterC failed write to replica\n");
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// reset / init voting state
void resetVotingState(int pipe_num) {
  for (int r_index = 0; r_index < REP_COUNT; r_index++) {
    replicas[r_index].voted[pipe_num] = false;
  }
}

void resetVotingState() {
  for (int p_index = 0; p_index < pipe_count; p_index++) {
    resetVotingState(p_index);
  }
}

void checkSend(int pipe_num) {
  bool all_reporting = true;
  for (int r_index = 0; r_index < REP_COUNT; r_index++) {
    // Check that all have reported
    all_reporting = all_reporting && replicas[r_index].voted[pipe_num];
  }

  if (!all_reporting) {
    return;
  }
  
  // Send the solution that at least two agree on
  for (int r_index = 0; r_index < REP_COUNT; r_index++) {
    if (memcmp(replicas[r_index].vot_pipes[pipe_num].buffer,
               replicas[(r_index+1) % REP_COUNT].vot_pipes[pipe_num].buffer,
               replicas[r_index].vot_pipes[pipe_num].buff_count) == 0) {
      int retval = write(ext_pipes[pipe_num].fd_out, replicas[r_index].vot_pipes[pipe_num].buffer,
            replicas[r_index].vot_pipes[pipe_num].buff_count);
      if (retval == 0) {
        perror("Seriously Voter?");
      }

      if (ext_pipes[pipe_num].type == MOV_CMD) {
        vote_stat = WAITING;
      }
      resetVotingState(pipe_num);

      return;
    }
  }

  printf("VoterC: No two replicas agreed.\n");
}

////////////////////////////////////////////////////////////////////////////////
// Process output from replica; vote on it
// TODO: right now voting only works for outgoing MOV_CMD
void processFromRep(int replica_num, int pipe_num) {
  int index = 0;

  if (replicas[replica_num].voted[pipe_num] == true) {
    printf("ERROR: Replica already voted\n");
  } else {
    // record vote
    replicas[replica_num].voted[pipe_num] = true;
  }

  checkSend(pipe_num);
}
