/*
 * An empty controller for debugging
 * This variation uses file descriptors for I/O (for now just ranger and command out).
 *
 * James Marshall
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "../include/controller.h"

#define PIPE_COUNT 2

struct typed_pipe pipes[PIPE_COUNT];
int pipe_count = PIPE_COUNT;
int read_in_index, write_out_index;

// TAS related
cpu_speed_t cpu_speed;
int priority;

const char* name = "Empty";

void enterLoop();

bool insertSDC = false;
void testSDCHandler(int signo) {
  insertSDC = true;
}

void setPipeIndexes() {
  read_in_index = 0;
  write_out_index = 1;
}

int parseArgs(int argc, const char **argv) {
  setPipeIndexes();
  // TODO: error checking
  priority = atoi(argv[1]);
  if (argc < 3) { // Must request fds
    pid_t pid = getpid();
    connectRecvFDS(pid, pipes, 2, "Empty");
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
      if (insertSDC) {
        insertSDC = false;
        commSendMoveCommand(pipes[write_out_index], 0.1, 1.0);
      } else {
        commSendMoveCommand(pipes[write_out_index], 0.1, 0.0);
      }
    } else if (read_ret == -1) {
      perror("Empty - read blocking");
      exit(-1);
    } else {
      puts("Empty read_ret == 0?");
      exit(-1);
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
