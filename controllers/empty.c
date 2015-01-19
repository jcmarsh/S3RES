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

#define PIPE_COUNT 2

struct typed_pipe pipes[PIPE_COUNT];
int pipe_count = PIPE_COUNT;
int read_in_index, write_out_index;

// TAS related
cpu_speed_t cpu_speed;
int priority;

const char* name = "Empty";

void enterLoop();
int initReplica();

void testSDCHandler(int signo) {
  // ignore
}

void setPipeIndexes() {
  read_in_index = 0;
  write_out_index = 1;
}

int parseArgs(int argc, const char **argv) {
  pid_t pid;

  setPipeIndexes();
  // TODO: error checking
  priority = atoi(argv[1]);
  if (argc < 3) { // Must request fds
    pid = getpid();
    //connectRecvFDS(pid, &read_in_fd, &write_out_fd, "Empty");
  } else {
    deserializePipe(argv[2], &pipes[read_in_index]);
    deserializePipe(argv[3], &pipes[write_out_index]);
  }

  return 0;
}

void enterLoop() {
  int read_ret;
  struct comm_range_pose_data recv_msg;
 
  while(1) {
    // Blocking, but that's okay with me
    read_ret = read(pipes[read_in_index].fd_in, &recv_msg, sizeof(struct comm_range_pose_data));
    if (read_ret > 0) {
      commSendMoveCommand(pipes[write_out_index], 0.1, 0.0);
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
