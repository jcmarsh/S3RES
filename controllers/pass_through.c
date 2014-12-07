/*
 * Simple filter that averages the three values previous values for
 * Ranger readings.
 *
 * James Marshall
 */

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/taslimited.h"
#include "../include/commtypes.h"
#include "../include/fd_client.h"

// Configuration parameters
#define PIPE_COUNT 2 // But sometimes 2; 2nd out pipe is optional

// pipe 0 is data in, 1 is filtered (averaged) out, 2 is just regular out
struct typed_pipe pipes[PIPE_COUNT];
int data_index, average_index;

// TAS related
cpu_speed_t cpu_speed;
int priority;

void enterLoop();
void command();
int initReplica();

void restartHandler(int signo) {
  // fork
  pid_t currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // child sets new id, recreates connects, loops
      for (int i = 0; i < PIPE_COUNT; i++) {
        resetPipe(&(pipes[i]));
      }

      if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
        perror("Failed to register the restart handler");
      }

      EveryTAS();
      
      // Get own pid, send to voter
      currentPID = getpid();
      connectRecvFDS(currentPID, pipes, PIPE_COUNT, "PassThrough");

      // unblock the signal
      sigset_t signal_set;
      sigemptyset(&signal_set);
      sigaddset(&signal_set, SIGUSR1);
      sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

      return;
    } else {   // Parent just returns
      waitpid(-1, NULL, WNOHANG);
      return;
    }
  } else {
    printf("Fork error!\n");
    return;
  }
}

int parseArgs(int argc, const char **argv) {
  // TODO: error checking
  priority = atoi(argv[1]);
  if (argc < 4) { // Must request fds
    pid_t currentPID = getpid();
    connectRecvFDS(currentPID, pipes, 2, "PassThrough");
    data_index = 0;
    average_index = 1;
  } else {
    data_index = 0;
    deserializePipe(argv[2], &pipes[data_index]);
    average_index = 1;
    deserializePipe(argv[3], &pipes[average_index]);
  }

  return 0;
}

// Should probably separate this out correctly
// Basically the init function
int initReplica() {
  InitTAS(DEFAULT_CPU, &cpu_speed, priority); // time

  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    perror("Failed to register the restart handler");
    return -1;
  }

  return 0;
}

void enterLoop() {
  int read_ret;
  struct comm_range_pose_data recv_msg;

  struct timeval select_timeout;
  fd_set select_set;

  while(1) {
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    FD_ZERO(&select_set);
    FD_SET(pipes[data_index].fd_in, &select_set);

    int retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    if (retval > 0) {
      if (FD_ISSET(pipes[data_index].fd_in, &select_set)) {
        read_ret = read(pipes[data_index].fd_in, &recv_msg, sizeof(struct comm_range_pose_data));
        if (read_ret > 0) {
          if (write(pipes[average_index].fd_out, &recv_msg, sizeof(struct comm_range_pose_data)) < sizeof(struct  comm_range_pose_data)) {
            perror("PassThough failed to write range data");
          }
        } else if (read_ret < 0) {
          perror("PassThough - read problems");
        } else {
          perror("PassThough read_ret == 0?");
        }
      }
    }
  }
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initReplica() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  enterLoop();
  return 0;
}
