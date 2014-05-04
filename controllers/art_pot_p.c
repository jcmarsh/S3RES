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
double ranger_ranges[365]; // 365 comes from somewhere in Player as the max.

int read_in_fd;
int write_out_fd;

char restart_byte[1] = {'*'};
int restart_fd[2];

// TAS related
cpu_speed_t cpu_speed;

timestamp_t last;

void enterLoop();
void command();
int initReplica();

void restartHandler(int signo) {
  // write a byte to the restart pipe
  write(restart_fd[1], restart_byte, 1);
}

// Need to restart a replica
void restart() {
  pid_t currentPID = 0;
  // fork
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // child sets new id, recreates connects, loops?
      // TODO: The pid is not known to the voter
      initReplica();
      connectRecvFDS(&read_in_fd, &write_out_fd);
      command(); // recalculate missed command
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

  // TODO: error checking
  if (argc < 3) { // Must request fds
    connectRecvFDS(&read_in_fd, &write_out_fd);
  } else {
    read_in_fd = atoi(argv[1]);
    write_out_fd = atoi(argv[2]);
  }

  return 0;
}

// Should probably separate this out correctly
// Basically the init function
int initReplica() {
  int flags = 0;

  InitTAS(DEFAULT_CPU, &cpu_speed);

  //  restart_byte = ;
  if (pipe(restart_fd) == -1) {
    perror("art_pot_p pipe error");
    return -1;
  }
  // Need to set to be non-blocking for reading.
  flags = fcntl(restart_fd[0], F_GETFL, 0);
  //  fcntl(restart_fd[0], F_SETFL, flags | O_NONBLOCK);
  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }
  return 0;
}

void command() {
  timestamp_t current;
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
      obs_x = ranger_ranges[i] * cos(tao);
      obs_y = ranger_ranges[i] * sin(tao);
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

#ifdef _STATS_CONT_COMMAND_
  current = generate_timestamp();
  
  printf("%lf\n", timestamp_to_realtime(current - last, cpu_speed));
#endif
#ifdef _STATS_CONT_TO_BENCH_
  printf("Cont\t%lf\n", timestamp_to_realtime(generate_timestamp(), cpu_speed));
#endif
  // Write move command
  write(write_out_fd, &message, sizeof(struct comm_header) + hdr.byte_count);
}

void requestWaypoints() {
  struct comm_header hdr;
  
  hdr.type = COMM_WAY_REQ;
  hdr.byte_count = 0;

  write(write_out_fd, &hdr, sizeof(struct comm_header));
}

void enterLoop() {
  void * update_id;
  int index;

  int read_ret;
  struct comm_header hdr;

  struct timeval select_timeout;
  fd_set select_set;
  int max_fd;

  while(1) {
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    max_fd = read_in_fd;
    FD_ZERO(&select_set);
    FD_SET(read_in_fd, &select_set);
    FD_SET(restart_fd[0], &select_set);
    if (restart_fd[0] > read_in_fd) {
      max_fd = restart_fd[0];
    }

    read_ret = select(max_fd + 1, &select_set, NULL, NULL, &select_timeout);
    if (read_ret > 0) {
      if (FD_ISSET(restart_fd[0], &select_set)) {
	read_ret = read(restart_fd[0], restart_byte, 1);
	if (read_ret > 0) {
	  // if it has something, restart  and exit loop (will get other read on next select
	  restart();
	} else if (read_ret < 0) {
	  // TODO: Can I ignore this error?
	}
      }
      if (FD_ISSET(read_in_fd, &select_set)) {
	read_ret = read(read_in_fd, &hdr, sizeof(struct comm_header));
	if (read_ret > 0) {
	  assert(read_ret == sizeof(struct comm_header));
	  switch (hdr.type) {
	  case COMM_RANGE_DATA:
	    //	printf("\tRecieved Range Data\n");
	    read_ret = read(read_in_fd, ranger_ranges, hdr.byte_count);
	    ranger_count = read_ret / sizeof(double);      
	    assert(read_ret == hdr.byte_count);
#ifdef _STATS_BENCH_TO_CONT_
	    printf("Cont\t%lf\n", timestamp_to_realtime(generate_timestamp(), cpu_speed));
#endif
#ifdef _STATS_CONT_COMMAND_
	    last = generate_timestamp();
#endif
	    // Calculates and sends the new command
	    command();
	    break;
	  case COMM_POS_DATA:
	    //	printf("\tRecieved Position Data\n");
	    read_ret = read(read_in_fd, pos, hdr.byte_count);
	    assert(read_ret == hdr.byte_count);
	    break;
	  case COMM_WAY_RES:
	    //	printf("\tRecieved Waypoint Response\n");
	    read_ret = read(read_in_fd, goal, hdr.byte_count);
	    assert(read_ret == hdr.byte_count);
	    break;
	  default:
	    // TODO: Fail? or drop data?
	    printf("ERROR: art_pot_p can't handle comm type: %d\n", hdr.type);
	  }
	} else if (read_ret == -1) {
	  perror("Blocking, eh?");
	} else {
	  puts("WHAT THE HELL DOES THIS MEAN?");
	}
      }
    }
  }
}

int main(int argc, const char **argv) {
  printf("SANITY? Sanity.\n");
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

