/*
 * Artificial Potential controller stand alone.
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

// Configuration parameters
#define VEL_SCALE 1
#define DIST_EPSILON .1
#define GOAL_RADIUS 0
#define GOAL_EXTENT 1
#define GOAL_SCALE 1
#define OBST_RADIUS 0
#define OBST_EXTENT 1
#define OBST_SCALE .3

// Controller state
bool active_goal;
double goal[3];

// Position
double pos[3];
int ranger_count;
double ranges[16]; // 16 is the size in commtypes.h

int read_in_fd;
int write_out_fd;

// TAS related
cpu_speed_t cpu_speed;

void enterLoop();
void command();
int initReplica();

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
      connectRecvFDS(currentPID, &read_in_fd, &write_out_fd);
      command(); // recalculate missed command TODO DON"T NEED
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
    connectRecvFDS(pid, &read_in_fd, &write_out_fd);
  } else {
    read_in_fd = atoi(argv[1]);
    write_out_fd = atoi(argv[2]);
  }

  return 0;
}

// Should probably separate this out correctly
// Basically the init function
int initReplica() {
  int scheduler;
  struct sched_param param;

  InitTAS(DEFAULT_CPU, &cpu_speed);

  scheduler = sched_getscheduler(0);
  printf("Art_Pot Scheduler: %d\n", scheduler);

  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }
  return 0;
}

void command() {
  double dist, theta, delta_x, delta_y, v, tao, obs_x, obs_y;
  double vel_cmd[2];
  int total_factors, i;
  
  struct comm_mov_cmd_msg message;
  struct comm_header hdr;

  // Head towards the goal! odom_pose: 0-x, 1-y, 2-theta
  dist = sqrt(pow(goal[INDEX_X] - pos[INDEX_X], 2)  + pow(goal[INDEX_Y] - pos[INDEX_Y], 2));
  theta = atan2(goal[INDEX_Y] - pos[INDEX_Y], goal[INDEX_X] - pos[INDEX_X]) - pos[INDEX_A];

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
  
  // TODO: Could I use goal_radius for the dist_epsilon
  // TODO: Now will not react to obstacles while at a waypoint. Even moving ones.
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
  } else { // within distance epsilon. Give it up, man.
    vel_cmd[0] = 0.0;
    vel_cmd[1] = 0.0;
  }

  hdr.type = COMM_MOV_CMD;
  hdr.byte_count = 2 * sizeof(double);
  message.hdr = hdr;
  message.vel_cmd[0] = vel_cmd[0];
  message.vel_cmd[1] = vel_cmd[1];

  // Write move command
  write(write_out_fd, &message, sizeof(struct comm_mov_cmd_msg));
}

void requestWaypoints() {
  struct comm_way_req_msg send_msg;
  
  send_msg.hdr.type = COMM_WAY_REQ;
  send_msg.hdr.byte_count = 0;

  write(write_out_fd, &send_msg, sizeof(struct comm_way_req_msg));
}

void enterLoop() {
  void * update_id;
  int index;

  int read_ret;
  struct comm_range_data_msg recv_msg;

  while(1) {
    // Blocking, but that's okay with me
    read_ret = read(read_in_fd, &recv_msg, sizeof(struct comm_range_data_msg));
    if (read_ret > 0) {
      switch (recv_msg.hdr.type) {
      case COMM_RANGE_DATA:
	ranger_count = 16;      
	for (index = 0; index < ranger_count; index++) {
	  ranges[index] = recv_msg.ranges[index];
	}
	// Calculates and sends the new command
	command();
	break;
      case COMM_POS_DATA:
	for (index = 0; index < 3; index++) {
	  pos[index] = ((struct comm_pos_data_msg*) (&recv_msg))->pose[index];
	}
	break;
      case COMM_WAY_RES:
	for (index = 0; index < 3; index++) {
	  goal[index] = ((struct comm_way_res_msg*) (&recv_msg))->point[index];
	}
	break;
      default:
	// TODO: Fail? or drop data?
	printf("ERROR: art_pot_p can't handle comm type: %d\n", recv_msg.hdr.type);
      }
    } else if (read_ret == -1) {
      perror("Blocking, eh?");
    } else {
      puts("ArtPot read_ret == 0?");
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

  requestWaypoints();

  enterLoop();

  return 0;
}

