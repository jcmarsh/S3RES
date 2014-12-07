/*
 * Author - James Marshall
 */

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <string.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../include/taslimited.h"
#include "../include/statstime.h"
#include "../include/replicas.h"
#include "../include/commtypes.h"
#include "../include/fd_server.h"
#include "../include/fd_client.h" // Used for testing

#define SIG SIGRTMIN + 7
#define REP_COUNT 3
#define INIT_ROUNDS 4
#define PERIOD_NSEC 120000 // Max time for voting in nanoseconds (120 micro seconds)

// Replica related data
struct replica replicas[REP_COUNT];

char* controller_name;
// pipes to external components (not replicas)
int pipe_count = 0;
struct typed_pipe ext_pipes[PIPE_LIMIT];

// Functions!
int initVoterC();
int parseArgs(int argc, const char **argv);
int main(int argc, const char **argv);
void doOneUpdate();
void processData(struct typed_pipe pipe, int pipe_index);
void resetVotingState();
void checkSend(int pipe_num, bool checkSDC);
void processFromRep(int replica_num, int pipe_num);
void restartReplica(int restarter, int restartee);

void timeout_sighandler(int signum) {
  printf("Timout\n");
  assert(write(timeout_fd[1], timeout_byte, 1) == 1);
}

void restartHandler() {
  printf("Handler Handling.\n");
  int restarter = rand() % REP_COUNT;
  int restartee = (restarter + (REP_COUNT -1)) % REP_COUNT;
  restartReplica(restarter, restartee);
}

void restartReplica(int restarter, int restartee) {
  // Kill old replica
  printf("Kill %d and repping %d\n", restartee, restarter);
  printf("killed pid: %d\t repp id: %d\n", replicas[restartee].pid, replicas[restarter].pid);
  kill(replicas[restartee].pid, SIGKILL); // Make sure it is dead.
  waitpid(-1, NULL, WNOHANG); // cleans up the zombie // Actually doesn't // Well, now it does.

  
  // cleanup replica data structure
  for (int i = 0; i < replicas[restartee].pipe_count; i++) {
    if (replicas[restartee].vot_pipes[i].fd_in > 0) {
      close(replicas[restartee].vot_pipes[i].fd_in);
    }
    if (replicas[restartee].vot_pipes[i].fd_out > 0) {
      close(replicas[restartee].vot_pipes[i].fd_out);
    }
    if (replicas[restartee].rep_pipes[i].fd_in > 0) {
      close(replicas[restartee].rep_pipes[i].fd_in);
    }
    if (replicas[restartee].rep_pipes[i].fd_out > 0) {
      close(replicas[restartee].rep_pipes[i].fd_out);
    }
  }

  int retval = kill(replicas[restarter].pid, SIGUSR1);
  if (retval < 0) {
    perror("VoterC Signal Problem");
  }

  // re-init failed rep, create pipes
  printf("Controller name: %s\n", controller_name);
  initReplicas(&(replicas[restartee]), 1, controller_name, voter_priority + 5);
  createPipes(&(replicas[restartee]), 1, ext_pipes, pipe_count);
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterC() {
  struct sigevent sev;
  sigset_t mask;

  // timeout_fd
  if (pipe(timeout_fd) == -1) {
    perror("VoterC time out pipe fail");
    return -1;
  }

  // create timer
  /* Establish handler for timer signal */
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
  memset(&sev, 0, sizeof(sev));
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIG;
  sev.sigev_value.sival_ptr = &timerid;
  if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
    perror("VoterC timer_create failed");
    return -1;
  }

  // Setup fd server
  //createFDS(&sd, controller_name);

  // Let's try to launch the replicas
  initReplicas(replicas, REP_COUNT, controller_name, voter_priority + 5);
  createPipes(replicas, REP_COUNT, ext_pipes, pipe_count);
  forkReplicas(replicas, REP_COUNT);
  
  srand(time(NULL));

  return 0;
}

int parseArgs(int argc, const char **argv) {
  controller_name = const_cast<char*>(argv[1]);
  voting_timeout = atoi(argv[2]);
  voter_priority = atoi(argv[3]);
  if (voting_timeout == 0) {
    voting_timeout = PERIOD_NSEC;
  }

  if (argc < 5) { // In testing mode
    pid_t currentPID = getpid();
    //pipe_count = 4;  // 4 is the only controller specific bit here... and ArtPotTest
    //connectRecvFDS(currentPID, ext_pipes, pipe_count, "ArtPotTest");
    pipe_count = 2;  // 4 is the only controller specific bit here... and ArtPotTest
    connectRecvFDS(currentPID, ext_pipes, pipe_count, "FilterTest");
    timer_start_index = 0;
    timer_stop_index = 1;
        // puts("Usage: VoterC <controller_name> <timeout> <message_type:fd_in:fd_out> <...>");
    // return -1;
  } else {
    for (int i = 0; (i < argc - 4 && i < PIPE_LIMIT); i++) {
      deserializePipe(argv[i + 4], &ext_pipes[pipe_count]);
      pipe_count++;
    }
    if (pipe_count >= PIPE_LIMIT) {
      printf("VoterC: Raise pipe limit.\n");
    }
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

  timer_started = true;

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
          ext_pipes[p_index].buff_count = read(read_fd, ext_pipes[p_index].buffer, 1024); // TODO remove magic number
          write(ext_pipes[1].fd_out, ext_pipes[p_index].buffer, ext_pipes[p_index].buff_count);
          //processData(ext_pipes[p_index], p_index);
        }
      }
    }
  } else if (errno == EINTR) {
    // Timer likely expired. Will loop back so no worries.
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
        perror("VoterC timer_settime failed");
      }
    }
  }

  for (int r_index = 0; r_index < REP_COUNT; r_index++) {
    int written = write(replicas[r_index].vot_pipes[pipe_index].fd_out, pipe.buffer, pipe.buff_count);
    if (written != pipe.buff_count) {
      printf("VoterC: bytes written: %d\texpected: %d\n", written, pipe.buff_count);
      perror("VoterC failed write to replica\n");
    }
  }
}
