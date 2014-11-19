/*
 * A Star controller
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

// Configuration parameters
#define GRID_NUM = 64

// Controller state
double goal[3] = {7.0, 7.0, 0.0};

// Position
double pose[3];

struct typed_pipe pipes[2]; // Map updates in 0, waypoints out 1

void enterLoop();
void command();
int initReplica();

// TODO: move to library
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
      connectRecvFDS(currentPID, pipes, 2, "AStar");

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
  // TODO: error checking
  if (argc < 3) { // Must request fds

  } else {
    deserializePipe(argv[1], &pipes[0]);
    deserializePipe(argv[2], &pipes[1]);
  }

  return 0;
}

// Should probably separate this out correctly
// Basically the init function
int initReplica() {
  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }
  return 0;
}

void command() {
  printf("AStar knows not what it does\n");
  // Write move command
  commSendWaypoints(pipes[1], goal[0], goal[1], goal[2]);
}

void enterLoop() {
  int read_ret;
  struct comm_map_update recv_msg;

  while(1) {
    // Blocking, but that's okay with me
    read_ret = read(pipes[0].fd_in, &recv_msg, sizeof(struct comm_map_update));
    if (read_ret > 0) {
      // Update with data?
      command();
    } else if (read_ret == -1) {
      perror("Blocking, eh?");
    } else {
      perror("ArtPot read_ret == 0?");
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

