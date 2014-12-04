/*
 * An empty controller for debugging
 * This variation uses file descriptors for I/O (for now just ranger and command out).
 *
 * James Marshall
 */

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "../include/taslimited.h"
#include "../include/commtypes.h"
#include "../include/statstime.h"
#include "../include/fd_client.h"

struct typed_pipe pipes[2];

// TAS related
cpu_speed_t cpu_speed;
int priority;

void enterLoop();
int initReplica();

void restartHandler(int signo) {
  pid_t currentPID = 0;
  // fork
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // child sets new id, recreates connects, loops
      initReplica();
      // Get own pid, send to voter
      currentPID = getpid();
      connectRecvFDS(currentPID, pipes, 2, "Empty");
      
      // unblock the signal
      sigset_t signal_set;
      sigemptyset(&signal_set);
      sigaddset(&signal_set, SIGUSR1);
      sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

      enterLoop(); // return to normal
    } else {   // Parent just returns
      return;
    }
  } else {
    printf("Fork error!\n");
    return;
  }
}

int parseArgs(int argc, const char **argv) {
  pid_t pid;

  // TODO: error checking
  priority = atoi(argv[1]);
  if (argc < 3) { // Must request fds
    pid = getpid();
    //connectRecvFDS(pid, &read_in_fd, &write_out_fd, "Empty");
  } else {
    deserializePipe(argv[2], &pipes[0]);
    deserializePipe(argv[3], &pipes[1]);
  }

  return 0;
}

// TODO: Should probably separate this out correctly
// Basically the init function
int initReplica() {
  InitTAS(DEFAULT_CPU, &cpu_speed, priority);

  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }
  return 0;
}

void enterLoop() {
  int read_ret;
  struct comm_range_pose_data recv_msg;
 
  while(1) {
    // Blocking, but that's okay with me
    read_ret = read(pipes[0].fd_in, &recv_msg, sizeof(struct comm_range_pose_data));
    if (read_ret > 0) {
      commSendMoveCommand(pipes[1], 0.1, 0.0);
    } else if (read_ret == -1) {
      perror("Empty - read blocking");
    } else {
      puts("Empty read_ret == 0?");
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
