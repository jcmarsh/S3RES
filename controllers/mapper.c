/*
 * Builds a map of obstacles
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

#define MAP_SIZE 16
#define OFFSET_X  8
#define OFFSET_Y  8
#define GRID_NUM 32

#define RANGE_COUNT 16

struct typed_pipe data_in;

FILE * out_file;

// TAS related
cpu_speed_t cpu_speed;

void enterLoop();
int initReplica();

struct point_i {
  int x;
  int y;
};

struct point_d {
  int x;
  int y;
};

bool obstacle_map[GRID_NUM][GRID_NUM];

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
      //connectRecvFDS(currentPID, &read_in_fd, &write_out_fd, "Mapper");
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
  int i;
  pid_t pid;

  // TODO: error checking
  if (argc < 3) { // Must request fds
    pid = getpid();
    //connectRecvFDS(pid, &read_in_fd, &write_out_fd, "Mapper");
  } else {
    deserializePipe(argv[1], &data_in);
  }

  return 0;
}

// TODO: Should probably separate this out correctly
// Basically the init function
int initReplica() {
  int scheduler;
  struct sched_param param;

  InitTAS(DEFAULT_CPU, &cpu_speed, 5);

  scheduler = sched_getscheduler(0);
  printf("Mapper Scheduler: %d\n", scheduler);

  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }
  return 0;
}

struct point_i gridify(struct point_d p) {
  struct point_i new_point;
  double interval = MAP_SIZE / GRID_NUM;
  new_point.x = (int)((p.x + OFFSET_X) / interval);
  new_point.y = (int)((p.y + OFFSET_Y) / interval);

  // Account for edge of map
  if (new_point.x == GRID_NUM) {
    new_point.x--;
  }
  if (new_point.y == GRID_NUM) {
    new_point.y--;
  }
  
  return new_point;
}

// return true if something changed
bool addObstacle(struct point_i obs) {
  if (obstacle_map[obs.x][obs.y]) {
    // obstacle already there, return false (no changes)
    return false;
  } else {
    obstacle_map[obs.x][obs.y] = true;
    return true;
  }
}

void updateMap(struct comm_range_pose_data * data) {
  double x_pose, y_pose, theta_pose;
  // Read pose
  x_pose = data->pose[INDEX_X];
  y_pose = data->pose[INDEX_Y];
  theta_pose = data->pose[INDEX_A];

  bool changed = false;

  // Convert ranges absolute positions
  for (int i = 0; i < RANGE_COUNT; i++) {
    struct point_d obstacle_l, obstacle_g;

    // obstacle location relative to the robot
    double tao = (2 * M_PI * i) / RANGE_COUNT;
    obstacle_l.x = data->ranges[i] * cos(tao);
    obstacle_l.y = data->ranges[i] * sin(tao);
    
    // obstacle location in global coords
    obstacle_g.x = obstacle_l.x * cos(theta_pose) - obstacle_l.y * sin(theta_pose);
    obstacle_g.x += x_pose;
    obstacle_g.y = obstacle_l.x * sin(theta_pose) + obstacle_l.y * cos(theta_pose);
    obstacle_g.y += y_pose;

    changed = addObstacle(gridify(obstacle_g));
  }

  // send new map out
  if (changed) {
    for (int i = 0; i < GRID_NUM; i++) {
      for (int j = 0; j < GRID_NUM; j++) {
        if (obstacle_map) {
          fprintf(out_file, "X");
        } else {
          fprintf(out_file, " ");
        }
      }
      fprintf(out_file, "\n");
    }
    fprintf(out_file, "\n");
  }
}

void enterLoop() {
  int read_ret;
  struct comm_range_pose_data recv_msg;

  // For now I'm goint to write out everything to a file
  out_file = fopen("map_output.txt", "w");
  if (out_file == NULL)
  {
    printf("Error opening file!\n");
    exit(1);
  }

  for (int i = 0; i < GRID_NUM; i++) {
    for (int j = 0; j < GRID_NUM; j++) {
      obstacle_map[i][j] = false;
    }
  }
 
  while(1) {
    // Blocking, but that's okay with me
    read_ret = read(data_in.fd_in, &recv_msg, sizeof(struct comm_range_pose_data));
    if (read_ret > 0) {
      // TODO: Error checking
      updateMap(&recv_msg);
    } else if (read_ret == -1) {
      perror("Blocking, eh?");
    } else {
      puts("Mapper read_ret == 0?");
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

