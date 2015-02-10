/*
 * Artificial Potential controller stand alone.
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

// Configuration parameters
#define VEL_SCALE 1
#define DIST_EPSILON .4
#define GOAL_RADIUS 0
#define GOAL_EXTENT .5
#define GOAL_SCALE 1
#define OBST_RADIUS .1
#define OBST_EXTENT .5
#define OBST_SCALE 1.5

#define PIPE_COUNT 4

// Controller state
double goal[] = {0.0, 0.0, 0.0};
double next_goal[] = {0.0, 0.0, 0.0};

// Position
double pos[3];
int ranger_count = 16;
double ranges[16]; // 16 is the size in commtypes.h

int pipe_count = PIPE_COUNT; // 4 with a planner, 2 otherwise
struct typed_pipe pipes[PIPE_COUNT]; // 0 is data_in, 1 is cmd_out
int data_index, out_index, way_req_index, way_res_index;

// TAS related
cpu_speed_t cpu_speed;
int priority;

const char* name = "ArtPot";

void enterLoop(void);
void command(void);

// Set indexes based on pipe types
void setPipeIndexes(void) {
  way_req_index = -1;
  way_res_index = -1;
  for (int i = 0; i < PIPE_COUNT; i++) {
    switch (pipes[i].type) {
      case RANGE_POSE_DATA:
        data_index = i;
        break;
      case MOV_CMD:
        out_index = i;
        break;
      case WAY_RES:
        way_res_index = i;
        break;
      case WAY_REQ:
        way_req_index = i;
        break;
    }
  }
}

bool insertSDC;
// Need a way to simulate SDC (rare)
void testSDCHandler(int signo) {
  insertSDC = true;
}

int parseArgs(int argc, const char **argv) {
  // TODO: error checking
  priority = atoi(argv[1]);
  if (argc < 4) { // Must request fds
    pid_t currentPID = getpid();
    if (connectRecvFDS(currentPID, pipes, PIPE_COUNT, name) < 0) {
      printf("Error in %s: failed on connectRecvFDS call. Exiting.\n", name);
      exit(-1);
    }
    setPipeIndexes();
  } else {
    for (int i = 0; (i < argc - 2) && (i < PIPE_COUNT); i++) {
      deserializePipe(argv[i + 2], &pipes[i]);
    }
    if (argc < 6) {
      pipe_count = 2; // no planner, now waypoint req/res
    } else {
      pipe_count = PIPE_COUNT;
    }
    setPipeIndexes();
  }

  return 0;
}

void command(void) {
  double dist, theta, delta_x, delta_y, v, tao, obs_x, obs_y;
  double vel_cmd[2];
  int total_factors, i;
  
  // Head towards the goal! odom_pose: 0-x, 1-y, 2-theta
  dist = sqrt(pow(goal[INDEX_X] - pos[INDEX_X], 2)  + pow(goal[INDEX_Y] - pos[INDEX_Y], 2));
  theta = atan2(goal[INDEX_Y] - pos[INDEX_Y], goal[INDEX_X] - pos[INDEX_X]) - pos[INDEX_A];

  if (dist <= DIST_EPSILON) { // Try next goal
    goal[INDEX_X] = next_goal[INDEX_X];
    goal[INDEX_Y] = next_goal[INDEX_Y];
    goal[INDEX_A] = next_goal[INDEX_A];

    commSendWaypointRequest(pipes[way_req_index]);

    dist = sqrt(pow(goal[INDEX_X] - pos[INDEX_X], 2)  + pow(goal[INDEX_Y] - pos[INDEX_Y], 2));
    theta = atan2(goal[INDEX_Y] - pos[INDEX_Y], goal[INDEX_X] - pos[INDEX_X]) - pos[INDEX_A];
  }

  total_factors = 0;
  if (dist < GOAL_RADIUS) {
    v = 0;
    delta_x = 0;
    delta_y = 0;
  } else if (GOAL_RADIUS <= dist && dist <= GOAL_EXTENT + GOAL_RADIUS) {
      v = GOAL_SCALE * (dist - GOAL_RADIUS);
      delta_x = v * cos(theta);
      delta_y = v * sin(theta);
      total_factors += 1;
  } else {
    v = GOAL_SCALE; //* goal_extent;
    delta_x = v * cos(theta);
    delta_y = v * sin(theta);
    total_factors += 1;
  }
  
  if (dist > DIST_EPSILON) {
    // Makes the assumption that scans are evenly spaced around the robot.
    for (i = 0; i < ranger_count; i++) {
      // figure out location of the obstacle...
      tao = (2 * M_PI * i) / ranger_count;
      obs_x = ranges[i] * cos(tao);
      obs_y = ranges[i] * sin(tao);
      // obs.x and obs.y are relative to the robot, and I'm okay with that.
      dist = sqrt(pow(obs_x, 2) + pow(obs_y, 2));
      theta = atan2(obs_y, obs_x);
    
      if (dist <= OBST_EXTENT + OBST_RADIUS) {
        delta_x += -OBST_SCALE * (OBST_EXTENT + OBST_RADIUS - dist) * cos(theta);
        delta_y += -OBST_SCALE * (OBST_EXTENT + OBST_RADIUS - dist) * sin(theta);
        total_factors += 1;
      }
    }

    delta_x = delta_x / total_factors;
    delta_y = delta_y / total_factors;
  
    vel_cmd[0] = sqrt(pow(delta_x, 2) + pow(delta_y, 2));
    vel_cmd[1] = atan2(delta_y, delta_x);
    vel_cmd[0] = VEL_SCALE * vel_cmd[0] * (abs(M_PI - vel_cmd[1]) / M_PI);
    vel_cmd[1] = VEL_SCALE * vel_cmd[1];
  } else { // within distance epsilon. And already tried next goal.
    vel_cmd[0] = 0.0;
    vel_cmd[1] = 0.0;
  }

  if (insertSDC) {
    vel_cmd[0] += 1;
    insertSDC = false;
  }

  // Write move command
  commSendMoveCommand(pipes[out_index], vel_cmd[0], vel_cmd[1]);
}

void enterLoop(void) {
  int read_ret;
  struct comm_range_pose_data recv_msg_data;
  struct comm_way_res recv_msg_way;

  struct timeval select_timeout;
  fd_set select_set;
  
  while(1) {
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    FD_ZERO(&select_set);
    FD_SET(pipes[data_index].fd_in, &select_set);
    if (PIPE_COUNT == pipe_count) {
      FD_SET(pipes[way_res_index].fd_in, &select_set);
    }

    int retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    if (retval > 0) {
      if (FD_ISSET(pipes[data_index].fd_in, &select_set)) {
        read_ret = read(pipes[data_index].fd_in, &recv_msg_data, sizeof(struct comm_range_pose_data));
        if (read_ret > 0) {
          // TODO check for erros
          commCopyRanger(&recv_msg_data, ranges, pos);
          // Calculates and sends the new command
          command();
        } else if (read_ret < 0) {
          perror("ArtPot - read problems");
        } else {
          perror("ArtPot read_ret == 0?");
        }
      }
      if (PIPE_COUNT == pipe_count) {
        if (FD_ISSET(pipes[way_res_index].fd_in, &select_set)) {
          read_ret = read(pipes[way_res_index].fd_in, &recv_msg_way, sizeof(struct comm_way_res));
          if (read_ret > 0) {
            // TODO check for erros
            commCopyWaypoints(&recv_msg_way, goal, next_goal);
          } else if (read_ret < 0) {
            perror("ArtPot - read problems");
          } else {
            perror("ArtPot read_ret == 0?");
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

  if (initReplica() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  insertSDC = false;
  if (PIPE_COUNT == pipe_count) {
    commSendWaypointRequest(pipes[way_req_index]);
  } else {
    goal[0] = 7.0;
    goal[1] = 7.0;
  }

  enterLoop();

  return 0;
}
