/*
 * Artificial Potential controller stand alone.
 * This variation uses file descriptors for I/O (for now just ranger and command out).
 *
 * James Marshall
 */

#include "controller.h"
#include "./inc/mapping.h"
#include <math.h>

// the blocks that make up the maze are 1.6x1.6
// With grid_num = 48, each grid is 0.33x0.33
// Configuration parameters
#if GRID_NUM==96
  #define VEL_SCALE 2
  #define DIST_EPSILON .4
  #define GOAL_RADIUS 0
  #define GOAL_EXTENT .3
  #define GOAL_SCALE 1
  #define OBST_RADIUS .2
  #define OBST_EXTENT .5
  #define OBST_SCALE 1
#elif GRID_NUM==80
  #define VEL_SCALE 2
  #define DIST_EPSILON .6
  #define GOAL_RADIUS 0
  #define GOAL_EXTENT .5
  #define GOAL_SCALE 1
  #define OBST_RADIUS .4
  #define OBST_EXTENT .7
  #define OBST_SCALE 1
#else  // GRID_NUM == 48
  #define VEL_SCALE 1.3
  #define DIST_EPSILON .3 // about the size of a grid square
  #define GOAL_RADIUS 0
  #define GOAL_EXTENT .2
  #define GOAL_SCALE 1
  #define OBST_RADIUS .3
  #define OBST_EXTENT .5
  #define OBST_SCALE 2
#endif

#define PIPE_COUNT 4

// Controller state
double goal[] = {0.0, 0.0, 0.0};
double next_goal[] = {0.0, 0.0, 0.0};

// Position
int seq_count = -1;
double pos[3];
int ranger_count = 16; // TODO: used what's in commtypes... already included I think
double ranges[16]; // 16 is the size in commtypes.h

bool way_pending = false;

int pipe_count = PIPE_COUNT; // 4 with a planner, 2 otherwise
struct typed_pipe pipes[PIPE_COUNT]; // 0 is data_in, 1 is cmd_out
int data_index, out_index, way_req_index, way_res_index;

// TAS related
int priority;
int pinned_cpu;

const char* name = "ArtPot";

void enterLoop(void);
void command(void);

// Set indexes based on pipe types
void setPipeIndexes(void) {
  int i;

  way_req_index = -1;
  way_res_index = -1;
  for (i = 0; i < pipe_count; i++) {
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

bool insertSDC = false;
bool insertCFE = false;

int parseArgs(int argc, const char **argv) {
  int i;

  // TODO: error checking
  priority = atoi(argv[1]);
  pipe_count = atoi(argv[2]);
  if (argc < 5) { // Must request fds
    pid_t currentPID = getpid();
    if (connectRecvFDS(currentPID, pipes, pipe_count, name, &pinned_cpu, &priority) < 0) {
      debug_print("Error in %s: failed on connectRecvFDS call. Exiting.\n", name);
      exit(-1);
    }
  } else {
    for (i = 0; (i < argc - 3) && (i < PIPE_COUNT); i++) {
      deserializePipe(argv[i + 3], &pipes[i]);
    }
  }
  setPipeIndexes();

  return 0;
}

void command(void) {
  double dist, theta, delta_x, delta_y, v, tao, obs_x, obs_y;
  double vel_cmd[2];
  int total_factors, i;
  bool request_way = false;

  // Head towards the goal! odom_pose: 0-x, 1-y, 2-theta
  dist = sqrt(pow(goal[INDEX_X] - pos[INDEX_X], 2) + pow(goal[INDEX_Y] - pos[INDEX_Y], 2));
  theta = atan2(goal[INDEX_Y] - pos[INDEX_Y], goal[INDEX_X] - pos[INDEX_X]) - pos[INDEX_A];

  if (dist <= DIST_EPSILON) { // Try next goal
    goal[INDEX_X] = next_goal[INDEX_X];
    goal[INDEX_Y] = next_goal[INDEX_Y];
    goal[INDEX_A] = next_goal[INDEX_A];

    request_way = true;

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

    if (fabs(vel_cmd[1]) < 1) { // Only go forward if mostly pointed at goal
      vel_cmd[0] = VEL_SCALE * vel_cmd[0];
    } else {
      vel_cmd[0] = 0.0;
    }
    vel_cmd[1] = VEL_SCALE * vel_cmd[1];
  } else { // within distance epsilon. And already tried next goal.
    vel_cmd[0] = 0.0;
    vel_cmd[1] = 0.0;
  }

  if (insertSDC) {
    insertSDC = false;
    vel_cmd[0] += 1;
  }

  //if (vel_cmd[0] < 0.1 && vel_cmd[1] < 0.1) {
  //  // Detect if robot is "stuck"
  //  request_way = true;
  //}

  if (!way_pending) {
    way_pending = true;
    if (request_way) {
      commSendWaypointRequest(&pipes[way_req_index], 1);
    } else {
      commSendWaypointRequest(&pipes[way_req_index], 0);
    }
  }

  // Write move command
  commSendMoveCommand(&pipes[out_index], seq_count, vel_cmd[0], vel_cmd[1]);
}

void enterLoop(void) {
  int read_ret;
  struct comm_range_pose_data recv_msg_data;
  struct comm_way_res recv_msg_way;

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
    if (PIPE_COUNT == pipe_count) {
      FD_SET(pipes[way_res_index].fd_in, &select_set);
    }

    int retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    if (retval > 0) {
      if (PIPE_COUNT == pipe_count) {
        if (FD_ISSET(pipes[way_res_index].fd_in, &select_set)) {
          read_ret = read(pipes[way_res_index].fd_in, &recv_msg_way, sizeof(struct comm_way_res));
          if (read_ret == sizeof(struct comm_way_res)) {
            way_pending = false;
            commCopyWaypoints(&recv_msg_way, goal, next_goal);
          } else if (read_ret > 0) {
            debug_print("ArtPot read way_res_index did not match expected size.\n");
          } else if (read_ret < 0) {
            debug_print("ArtPot - read way_res_index problems.\n");
          } else {
            debug_print("ArtPot read_ret == 0 on way_res_index.\n");
          } 
        }
      }
      if (FD_ISSET(pipes[data_index].fd_in, &select_set)) {
        read_ret = read(pipes[data_index].fd_in, &recv_msg_data, sizeof(struct comm_range_pose_data));
        if (read_ret == sizeof(struct comm_range_pose_data)) {
	  int old_seq = seq_count;
          commCopyRanger(&recv_msg_data, &seq_count, ranges, pos);
	  if (old_seq + 1 != seq_count) {
	    fputs("ART_POT SEQ ERROR.\n", stderr);
	    // fprintf(stderr, "ART_POT SEQ ERROR: %d - %d\n", old_seq + 1, seq_count);
	  }
          // Calculates and sends the new command
          command();
        } else if (read_ret > 0) {
          debug_print("ArtPot read data_index did not match expected size.\n");
        } else if (read_ret < 0) {
          debug_print("ArtPot - read data_index problems.\n");
        } else {
          debug_print("ArtPot read_ret == 0 on data_index.\n");
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

  // Set goal to initial start position (to force request).
  goal[0] = -7.0;
  goal[1] = -7.0;

  enterLoop();

  return 0;
}
