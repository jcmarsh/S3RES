/*
 * Records range and position data to a log file.
 *
 * James Marshall
 */

#include "controller.h"
#include "tas_time.h"
#include "bench_config.h"
#include "system_config.h"
#include <math.h>
#include <time.h>
#include <stdio.h>

#define GOAL_X      7.0
#define GOAL_Y      7.0
#define PIPE_COUNT  5

int pipe_count = PIPE_COUNT;

int seq_count;
double ranges[RANGER_COUNT] = {0};
// range and pose data is sent together...
double pose[3];

// pipe 0 is data in, 1 is filtered (averaged) out, 2 is just regular out
struct typed_pipe pipes[PIPE_COUNT];
int data_index;

// TAS related
int priority;
int pinned_cpu;

const char* name = "Logger";

FILE *log_file;

void enterLoop();
void command();

bool insertSDC = false;
bool insertCFE = false;

void setPipeIndexes(void) {
  data_index = 0;
}

int parseArgs(int argc, const char **argv) {
  setPipeIndexes();
  // TODO: Check for errors
  priority = atoi(argv[1]);
  if (argc < 4) { // Must request fds
    // printf("Usage: Logger <priority> <pipe_num> <pipe_in>\n");
    pid_t currentPID = getpid();
    connectRecvFDS(currentPID, pipes, 1, name, &pinned_cpu, &priority); // TODO: lame. What about strings to print?
  } else {
    deserializePipe(argv[3], &pipes[data_index]);
    int i;
    pipe_count = 1;
    for (i = 4; i < argc; i++) {
      if (pipe_count >= PIPE_COUNT) {
        debug_print("ERROR: Logger needs to raise pipe limit.\n");
        return -1;
      }
      deserializePipe(argv[i], &pipes[pipe_count]);
      pipe_count++;
    }
  }

  return 0;
}

bool finished = false;
double prev_x = -7.0, prev_y = -7.0;
timestamp_t prev_time = 0;
double time_elapsed = 0.0;
void command(void) {
  int index = 0;
  timestamp_t current_time = generate_timestamp(); // TODO lookup timestamps

  // find minimum obs distance
  // calculate speed
  double distance_goal = sqrt(((pose[0] - GOAL_X) * (pose[0] - GOAL_X)) + ((pose[1] - GOAL_Y) * (pose[1] - GOAL_Y)));
  if (distance_goal < 1) { // Robot stops short
    // Skip; too close to end
    finished = true;
  } else if (prev_time == 0) {
    // Skip; first cycle 
  } else {
    // calc velocity
    double velocity = 0.0;
    double distance = sqrt(((pose[0] - prev_x) * (pose[0] - prev_x)) + ((pose[1] - prev_y) * (pose[1] - prev_y)));
    double time_this_round = diff_time(current_time, prev_time, (1000000 * CPU_MHZ)); // diff_time normally returns usec, multiplying CPU_MHZ by 1 mil gives seconds
    time_elapsed = time_elapsed + time_this_round;

    if (pose[0] == prev_x && pose[1] == prev_y) {
      velocity = 0.0;
    } else {
      velocity = distance / time_elapsed;
      time_elapsed = 0.0;
    }

    // obstacle distance
    double min = 1000; // approximately infinite.
    for (index = 0; index < RANGER_COUNT; index++) {
      if (ranges[index] < min) {
        min = ranges[index];
      }
    }

    fprintf(log_file, "(%f,\t%f,\t%f,\t%f,\t%f,\t%f,\t%d)\n", min, velocity, distance, time_this_round, pose[0], pose[1], seq_count);
  }
  prev_time = current_time;
  prev_x = pose[0];
  prev_y = pose[1];
}

void enterLoop(void) {
  int read_ret;
  struct comm_range_pose_data recv_msg;

  struct timeval select_timeout;
  fd_set select_set;

  while(1) {
    if (insertSDC || insertCFE) {
      printf("ERROR: no errors should be inserted into the logger component!\n");
    }

    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    FD_ZERO(&select_set);
    FD_SET(pipes[data_index].fd_in, &select_set);

    int i;
    for (i = 1; i < pipe_count; i++) {
      FD_SET(pipes[i].fd_in, &select_set);
    }

    int retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    if (retval > 0) {
      if (FD_ISSET(pipes[data_index].fd_in, &select_set)) {
        read_ret = read(pipes[data_index].fd_in, &recv_msg, sizeof(struct comm_range_pose_data));
        if (read_ret == sizeof(struct comm_range_pose_data)) {
          commCopyRanger(&recv_msg, &seq_count, ranges, pose);
          // Calculates and sends the new command
          command();
        } else if (read_ret > 0) {
          printf("Logger read data_index did not match expected size.\n");
        } else if (read_ret < 0) {
          perror("Logger - read data_index problems");
        } else {
          perror("Logger read_ret == 0 on data_index");
        }
      }
      for (i = 1; i < pipe_count; i++) {
        if (FD_ISSET(pipes[i].fd_in, &select_set)) {
          struct comm_msg_buffer msg;
          commRecvMsgBuffer(&pipes[i], &msg);
          if(!finished) {
            fprintf(log_file, "LOGGED MSG: %s", msg.message);
          }
          free(msg.message);
        }
      }
    }
  }
}

// Create a log file
int openFile(void) {
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  char * file_name;

  if (asprintf(&file_name, "./%02d-%02d_%02d-%02d-%02d_log.txt", tm.tm_mday, tm.tm_mon + 1, tm.tm_hour, tm.tm_min, tm.tm_sec) < 0) {
    perror("Logger failed to create file name char*");
    return -1;
  }

  log_file = fopen(file_name, "w");
  if (log_file == NULL) {
    perror("Logger failed open file");
    return -1;
  }

  fprintf(log_file, "(min_dist,\tvelocity,\tdistance,\ttime_elapsed,\tX position,\tY position\n");
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    printf("Logger ERROR: failure parsing args.\n");
    return -1;
  }

  if (initController() < 0) {
    printf("Logger ERROR: failure in setup function.\n");
    return -1;
  }

  if (openFile() <  0) {
    printf("Logger ERROR: failure in findOpenFile.\n");
    return -1;
  }

  enterLoop();
  return 0;
}
