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
#include "../include/statstime.h"
#include "../include/fd_client.h"

// Configuration parameters
#define WINDOW_SIZE 3
#define RANGER_COUNT 16  // 16 is the size in commtypes.h
#define PIPE_COUNT 3 // But sometimes 2; 2nd out pipe is optional

int pipe_count = PIPE_COUNT;
int window_index = 0;
double ranges[WINDOW_SIZE][RANGER_COUNT] = {0};
// range and pose data is sent together...
double pose[3];

// pipe 0 is data in, 1 is filtered (averaged) out, 2 is just regular out
struct typed_pipe pipes[PIPE_COUNT];
int data_index, average_index, regular_index;

// TAS related
cpu_speed_t cpu_speed;
int priority;

const char* name = "Filter";

void enterLoop();
void command();
int initReplica();

bool insertSDC;
// Need a way to simulate SDC (rare)
void testSDCHandler(int signo) {
  insertSDC = true;
}

void setPipeIndexes(void) {
  data_index = 0;
  average_index = 1;
  regular_index = 2;
}

int parseArgs(int argc, const char **argv) {
  setPipeIndexes();
  // TODO: Check for errors
  priority = atoi(argv[1]);
  if (argc < 4) { // Must request fds
    // printf("Usage: Filter <pipe_in> <pipe_out_0> <pipe_out_1>\n");
    pid_t currentPID = getpid();
    connectRecvFDS(currentPID, pipes, PIPE_COUNT, name);
    pipe_count = PIPE_COUNT;
  } else {
    deserializePipe(argv[2], &pipes[data_index]);
    deserializePipe(argv[3], &pipes[average_index]);
    if (5 == argc) {
      deserializePipe(argv[4], &pipes[regular_index]);
    } else {
      pipe_count = PIPE_COUNT - 1;
    }
  }

  return 0;
}

void command(void) {
  double range_average[RANGER_COUNT] = {0};

  if (insertSDC) {
    insertSDC = false;
    ranges[0][0]++;
  }

  for (int j = 0; j < RANGER_COUNT; j++) {
    for (int i = 0; i < WINDOW_SIZE; i++) {
      range_average[j] += ranges[i][j];
    }
    range_average[j] = range_average[j] / WINDOW_SIZE;
  }

  // Write out averaged range data (with pose)
  commSendRanger(pipes[average_index], range_average, pose);
  if (PIPE_COUNT == pipe_count) {
    commSendRanger(pipes[regular_index], ranges[(window_index + (WINDOW_SIZE - 1)) % WINDOW_SIZE], pose);
  }
}

void enterLoop(void) {
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
          // TODO: Error checking
          commCopyRanger(&recv_msg, ranges[window_index], pose);
          window_index = (window_index + 1) % WINDOW_SIZE;
          // Calculates and sends the new command
          command();
        } else if (read_ret < 0) {
          perror("Filter - read problems");
        } else {
          perror("Filter read_ret == 0?");
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
