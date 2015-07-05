/*
 * An empty controller for debugging
 * This variation uses file descriptors for I/O (for now just ranger and command out).
 *
 * James Marshall
 */

#include "controller.h"

#define PIPE_COUNT 2

struct typed_pipe pipes[PIPE_COUNT];
int pipe_count = PIPE_COUNT;
int read_in_index, write_out_index;

char *filler;

// TAS related
int priority;

const char* name = "Empty";

bool insertSDC = false;
bool insertCFE = false;

void setPipeIndexes(void) {
  read_in_index = 0;
  write_out_index = 1;
}

int parseArgs(int argc, const char **argv) {
  setPipeIndexes();
  // TODO: error checking
  priority = atoi(argv[1]);
  pipe_count = 2; // For now always 2
  if (argc < 5) { // Must request fds
    pid_t pid = getpid();
    connectRecvFDS(pid, pipes, pipe_count, "Empty");
  } else {
    deserializePipe(argv[3], &pipes[read_in_index]);
    deserializePipe(argv[4], &pipes[write_out_index]);
  }

  return 0;
}

void enterLoop(void) {
  int read_ret;
  struct comm_range_pose_data recv_msg;

  struct timeval select_timeout;
  fd_set select_set;
 
  while(1) {
    if (insertCFE) {
      while (1) { }
    }
        
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    FD_ZERO(&select_set);
    FD_SET(pipes[read_in_index].fd_in, &select_set);

    // Blocking, but that's okay with me
    int retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    if (retval > 0) {
      if (FD_ISSET(pipes[read_in_index].fd_in, &select_set)) {
        read_ret = TEMP_FAILURE_RETRY(read(pipes[read_in_index].fd_in, &recv_msg, sizeof(struct comm_range_pose_data)));
        if (read_ret == sizeof(struct comm_range_pose_data)) {
          if (insertSDC) {
            insertSDC = false;
            commSendMoveCommand(&pipes[write_out_index], 0.1, 1.0);
          } else {
            commSendMoveCommand(&pipes[write_out_index], 0.1, 0.0);
          }
        } else if (read_ret > 0) {
          printf("Empty read read_in_index did not match expected size.\n");
        } else if (read_ret < 0) {
          perror("Empty - read read_in_index problems");
        } else {
          perror("Empty read_ret == 0 on read_in_index");
        } 
      }
    }
  }
}

int main(int argc, const char **argv) {
  //filler = malloc(sizeof(char) * 8896 * 1024 + (sizeof(char) * 150000 * 1024)); // for 160,000 kB
  
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initController() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  enterLoop();

  return 0;
}
