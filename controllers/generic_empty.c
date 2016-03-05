/*
 * An actual ping-pong controller. Reads messages on one pipe and spits them out over the other.
 *
 * James Marshall
 */

#include "controller.h"

#define PIPE_COUNT 2

struct typed_pipe pipes[PIPE_COUNT];
int pipe_count = PIPE_COUNT;
int read_in_index, write_out_index;

// TAS related
int priority;
int pinned_cpu;

const char* name = "GenericEmpty";

bool insertSDC = false;
bool insertCFE = false;

void setPipeIndexes(void) {
  read_in_index = 0;
  write_out_index = 1;
}

int parseArgs(int argc, const char **argv) {
  setPipeIndexes();

  if (argc < 2) {
    puts("Usage: GenericEmpty <priority> <optional pipes...>\n");
  }
  // TODO: error checking
  priority = atoi(argv[1]);
  pipe_count = 2; // For now always 2
  if (argc < 4) { // Must request fds
    pid_t pid = getpid();
    connectRecvFDS(pid, pipes, pipe_count, "GenericEmpty", &pinned_cpu);
  } else {
    deserializePipe(argv[2], &pipes[read_in_index]);
    deserializePipe(argv[3], &pipes[write_out_index]);
  }

  return 0;
}

void enterLoop(void) {
  int read_ret, write_ret;
  char buffer[4096] = {0}; // Same as MAX_TYPED_PIPE_BUFF and VOTE_BUFF

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
        read_ret = read(pipes[read_in_index].fd_in, buffer, sizeof(buffer));
        if (read_ret > 0) {
          write_ret = write(pipes[write_out_index].fd_out, buffer, sizeof(char) * read_ret);
          if (read_ret != write_ret) {
            debug_print("GenericEmpty failed to write as much as it read: %d read to %d write.\n", read_ret, write_ret);
          }
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

  if (initController() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  enterLoop();

  return 0;
}
