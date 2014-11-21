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
#include <unistd.h>

#include "../include/taslimited.h"
#include "../include/commtypes.h"
#include "../include/statstime.h"
#include "../include/fd_client.h"

// Configuration parameters
#define WINDOW_SIZE 3
#define RANGER_COUNT 16  // 16 is the size in commtypes.h

int window_index = 0;
double ranges[WINDOW_SIZE][RANGER_COUNT] = {0};
// range and pose data is sent together...
double pose[3];

int pipe_count = 3;
// pipe 0 is data in, 1 is filtered (averaged) out, 2 is just regular out
struct typed_pipe pipes[3];

// TAS related
cpu_speed_t cpu_speed;

void enterLoop();
void command();
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
      connectRecvFDS(currentPID, pipes, pipe_count, "Filter");

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
    printf("Usage: Filter <pipe_in> <pipe_out_0> <pipe_out_1>\n");
  } else {
    deserializePipe(argv[1], &pipes[0]);
    for (int i = 2; (i <= pipe_count && i < argc); i++) {
      deserializePipe(argv[i], &pipes[i - 1]);
    }
  }

  return 0;
}

// Should probably separate this out correctly
// Basically the init function
int initReplica() {
  int scheduler;
  struct sched_param param;

  InitTAS(DEFAULT_CPU, &cpu_speed, 4);

  scheduler = sched_getscheduler(0);

  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }
  return 0;
}

void command() {
  double range_average[RANGER_COUNT] = {0};

  for (int j = 0; j < RANGER_COUNT; j++) {
    for (int i = 0; i < WINDOW_SIZE; i++) {
      range_average[j] += ranges[i][j];
    }
    range_average[j] = range_average[j] / WINDOW_SIZE;
  }

  // Write out averaged range data (with pose)
  commSendRanger(pipes[1], range_average, pose);
  // TODO: check that this exists before write
  // TODO: write the most recent; this data could be stale
  commSendRanger(pipes[2], ranges[WINDOW_SIZE - 1], pose);
}

void enterLoop() {
  void * update_id;
  int index;

  int read_ret;
  struct comm_range_pose_data recv_msg;

  while(1) {
    // Blocking, but that's okay with me
    read_ret = read(pipes[0].fd_in, &recv_msg, sizeof(struct comm_range_pose_data));
    if (read_ret > 0) {
      // TODO: Error checking
      commCopyRanger(&recv_msg, ranges[window_index], pose);
      window_index = (window_index + 1) % WINDOW_SIZE;
      // Calculates and sends the new command
      command();
    } else if (read_ret == -1) {
      perror("Filter - read blocking");
    } else {
      perror("Filter read_ret == 0?");
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

