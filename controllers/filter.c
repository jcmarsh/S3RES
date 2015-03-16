/*
 * Simple filter that averages the three values previous values for
 * Ranger readings.
 *
 * James Marshall
 */

#include "../include/controller.h"

// Configuration parameters
#define WINDOW_SIZE 3
#define PIPE_COUNT 4 // But sometimes 2, sometimes 3

int pipe_count = PIPE_COUNT;
double ranges[RANGER_COUNT] = {0};
// range and pose data is sent together...
double pose[3];

// pipe 0 is data in, 1 is filtered (averaged) out, 2 is just regular out
struct typed_pipe pipes[PIPE_COUNT];
int data_index;
int out_index[PIPE_COUNT - 1];

// TAS related
int priority;

const char* name = "Filter";

void enterLoop();
void command();

bool insertSDC = false;
void testSDCHandler(int signo, siginfo_t *si, void *unused) {
  insertSDC = true;
}

void setPipeIndexes(void) {
  int i;
  data_index = 0;
  for (i = 1; i < PIPE_COUNT; i++) {
    out_index[i - 1] = i;
  }
}

int parseArgs(int argc, const char **argv) {
  setPipeIndexes();
  int i;

  // TODO: Check for errors
  priority = atoi(argv[1]);
  pipe_count = atoi(argv[2]);
  if (argc < 5) { // Must request fds
    // printf("Usage: Filter <priority> <pipe_num> <pipe_in> <pipe_out_0> <pipe_out_1>\n");
    pid_t currentPID = getpid();
    connectRecvFDS(currentPID, pipes, pipe_count, name);
  } else {
    deserializePipe(argv[3], &pipes[data_index]);
    for (i = 4; i < 4 + pipe_count - 1; i++) {
      deserializePipe(argv[i], &pipes[out_index[i - 4]]);
    }
  }

  return 0;
}

void command(void) {
  int i;

  if (insertSDC) {
    insertSDC = false;
    ranges[0]++;
  }

  // Write out averaged range data (with pose)
  for (i = 1; i < pipe_count; i++) {
    commSendRanger(pipes[out_index[i - 1]], ranges, pose);
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
        read_ret = TEMP_FAILURE_RETRY(read(pipes[data_index].fd_in, &recv_msg, sizeof(struct comm_range_pose_data)));
        if (read_ret == sizeof(struct comm_range_pose_data)) {
          commCopyRanger(&recv_msg, ranges, pose);
          // Calculates and sends the new command
          command();
        } else if (read_ret > 0) {
          printf("Filter read data_index did not match expected size.\n");
        } else if (read_ret < 0) {
          perror("Filter - read data_index problems");
        } else {
          perror("Filter read_ret == 0 on data_index");
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
