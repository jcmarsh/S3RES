/*
 * Meant to stress the cpu and record it's resource usage.
 * Supposed to be based off of sensor data... so it will have range / pose updates.
 *
 * James Marshall
 */

#include "controller.h"

#include <sys/time.h>
#include <sys/resource.h>

#define PIPE_COUNT 1

struct typed_pipe pipes[PIPE_COUNT];
int pipe_count = PIPE_COUNT;
int read_in_index;

// TAS related
int priority;

const char* name = "Load";

bool insertCFE = false;
bool insertSDC = false; // Not used

void setPipeIndexes(void) {
  read_in_index = 0;
}

int parseArgs(int argc, const char **argv) {
  setPipeIndexes();
  // TODO: error checking
  priority = atoi(argv[1]);
  pipe_count = 1;
  if (argc < 4) { // Must request fds
    pid_t pid = getpid();
    connectRecvFDS(pid, pipes, pipe_count, "Load");
  } else {
    deserializePipe(argv[3], &pipes[read_in_index]);
  }

  return 0;
}

// TODO: Need to make more easily tunable.
// TODO: How to best report performance? Printf? Print to file?
void perCycleLoad(void) {
  struct rusage usage_stats;
  struct timeval prev_utime;

  int prime_count = 0;
  int n = 5000; // calculate primes up to n;

  // Get rusage
  getrusage(RUSAGE_SELF, &usage_stats);
  prev_utime.tv_sec = usage_stats.ru_utime.tv_sec;
  prev_utime.tv_usec = usage_stats.ru_utime.tv_usec;
  
  // do calculations
  int i, j;
  for (i = 3; i < n; i++) {
    bool prime = true;
    for (j = 2; j <= (i / 2); j++) { // Should be sqrt
      if ((i / (double)j) == (i / j)) {
        prime = false;
      }
    }
    if (prime) {
      prime_count++;
      // printf("Prime: %d\n", i);
    }
  }

  // Get rusage / output it
  getrusage(RUSAGE_SELF, &usage_stats);
  printf("Load found %d primes in: %ld - %ld\n", prime_count, usage_stats.ru_utime.tv_sec - prev_utime.tv_sec, usage_stats.ru_utime.tv_usec - prev_utime.tv_usec);
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
          perCycleLoad();
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
