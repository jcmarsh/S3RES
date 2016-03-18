/*
 * Builds a map of obstacles
 *
 * James Marshall
 */

#include "controller.h"
#include <math.h>
#include "./inc/mapping.h" // TODO: fix

#define RANGE_COUNT 16
#define PIPE_COUNT 3
#define OBS_THRES 2

struct typed_pipe pipes[PIPE_COUNT];
int pipe_count = PIPE_COUNT;
int data_index, update_index, ack_index; // Data from range pose, updates to planner, planner acks.

// TAS related
int priority;
int pinned_cpu;

struct point_i* current_pose;
// Count to 3 method worked great before
int obstacle_map[GRID_NUM][GRID_NUM];

struct comm_map_update send_msg;

const char* name = "Mapper";

void enterLoop(void);

void setPipeIndexes(void) {
  int i;
  for (i = 0; i < PIPE_COUNT; i++) {
    switch (pipes[i].type) {
      case RANGE_POSE_DATA:
        data_index = i;
        break;
      case MAP_UPDATE:
        update_index = i;
        break;
      case COMM_ACK:
        ack_index = i;
        break;
    }
  }
}

bool insertSDC = false;
bool insertCFE = false;

int parseArgs(int argc, const char **argv) {
  int i;

  if (argc < 3) {
    puts("Usage: Mapper <priority> <pipe_count> <pipes...>\n");
    exit(0);
  }
  // TODO: error checking
  priority = atoi(argv[1]);
  pipe_count = atoi(argv[2]); // For now always 3
  if (argc < 6) { // Must request fds
    pid_t currentPID = getpid();
    connectRecvFDS(currentPID, pipes, PIPE_COUNT, "Mapper", &pinned_cpu, &priority);
    //connectRecvFDS(currentPID, pipes, PIPE_COUNT, "MapperTest");
    setPipeIndexes();
  } else {
    for (i = 0; (i < argc - 3) && (i < PIPE_COUNT); i++) {
      deserializePipe(argv[i + 3], &pipes[i]);
    }
    setPipeIndexes();
  }

  return 0;
}

// return true if something changed
bool addObstacle(struct point_i* obs) {
  if (obs->x < 0 || obs->x > GRID_NUM || obs->y < 0 || obs->y > GRID_NUM) {
    // erroneous, ignore
    free(obs);
    return false;
  }
  if (obstacle_map[obs->x][obs->y] > OBS_THRES) {
    // obstacle already there, return false (no changes)
    free(obs);
    return false;
  } else {
    obstacle_map[obs->x][obs->y]++;
    if (obstacle_map[obs->x][obs->y] > OBS_THRES) {
      send_msg.obs_x[send_msg.obs_count] = obs->x;
      send_msg.obs_y[send_msg.obs_count] = obs->y;
      send_msg.obs_count++;
    }
    free(obs);
    return true;
  }
}

void updateMap(struct comm_range_pose_data * data) {
  int i;
  double theta_pose;
  // Read pose
  struct point_d pose;
  pose.x = data->pose[INDEX_X];
  pose.y = data->pose[INDEX_Y];
  theta_pose = data->pose[INDEX_A];

  free(current_pose);
  current_pose = gridify(&pose);
  send_msg.obs_count = 0;

  // Convert ranges absolute positions
  for (i = 0; i < RANGE_COUNT; i++) {
    struct point_d obstacle_l, obstacle_g;

    // obstacle location relative to the robot
    double tao = (2.0 * M_PI * i) / RANGE_COUNT;
    obstacle_l.x = data->ranges[i] * cos(tao);
    obstacle_l.y = data->ranges[i] * sin(tao);
    
    // obstacle location in global coords
    obstacle_g.x = obstacle_l.x * cos(theta_pose) - obstacle_l.y * sin(theta_pose);
    obstacle_g.x += pose.x;
    obstacle_g.y = obstacle_l.x * sin(theta_pose) + obstacle_l.y * cos(theta_pose);
    obstacle_g.y += pose.y;

    addObstacle(gridify(&obstacle_g));
  }

  // TODO: What if a SDC messed with the obs_count sent?
  if (insertSDC) {
    insertSDC = false;
    current_pose->x++;
  }
  send_msg.pose_x = current_pose->x;
  send_msg.pose_y = current_pose->y;

  commSendMapUpdate(&pipes[update_index], &send_msg);
}

void enterLoop(void) {
  int read_ret;
  struct comm_range_pose_data recv_msg;
  struct comm_ack ack_msg;

  struct timeval select_timeout;
  fd_set select_set;
 
  while(1) {
    if (insertCFE) {
      while (1) { }
    }
    
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    FD_ZERO(&select_set);
    FD_SET(pipes[data_index].fd_in, &select_set);
    FD_SET(pipes[ack_index].fd_in, &select_set);

    int retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    if (retval > 0) {
      if (FD_ISSET(pipes[data_index].fd_in, &select_set)) {
        read_ret = read(pipes[data_index].fd_in, &recv_msg, sizeof(struct comm_range_pose_data));
        if (read_ret == sizeof(struct comm_range_pose_data)) {
          updateMap(&recv_msg);
        } else if (read_ret > 0) {
          debug_print("Mapper read data_index did not match expected size: %d\n", read_ret);
        } else if (read_ret < 0) {
          debug_print("Mapper - read data_index problems.\n");
        } else {
          debug_print("Mapper read_ret == 0 on data_index.\n");
        }
      }
      if (FD_ISSET(pipes[ack_index].fd_in, &select_set)) {
        read_ret = read(pipes[ack_index].fd_in, &ack_msg, sizeof(struct comm_ack));
        if (read_ret == sizeof(struct comm_ack)) {
          // Do nothing
        } else if (read_ret > 0) {
          debug_print("Mapper read ack_index did not match expected size: %d\n", read_ret);
        } else if (read_ret < 0) {
          debug_print("Mapper - read ack_index problems.\n");
        } else {
          debug_print("Mapper read_ret == 0 on ack_index.\n");
        }
      }
    }
  }
}

int main(int argc, const char **argv) {
  int i, j;
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initController() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  send_msg.pose_x = 0;
  send_msg.pose_y = 0;
  send_msg.obs_count = 0;
  send_msg.obs_x = (int*)malloc(sizeof(int) * 128);
  send_msg.obs_y = (int*)malloc(sizeof(int) * 128);

  // Remember, don't initialize anything in the loop! (this used to be there... problems)
  for (i = 0; i < GRID_NUM; i++) {
    for (j = 0; j < GRID_NUM; j++) {
      obstacle_map[i][j] = 0;
    }
  }

  enterLoop();

  return 0;
}

