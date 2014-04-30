/*
 * Artificial Potential controller (instead of Threaded Driver)
 * This variation uses file descriptors for I/O (for now just ranger and command out).
 */

#include <libplayerc/playerc.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "../include/taslimited.h"
#include "../include/commtypes.h"

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

char ip_address[17];
char port_number[10];
int id_number;
int read_in_fd;
int write_out_fd;

// TAS related
cpu_speed_t cpu_speed;

void enterLoop();
void command();
int createConnections(char * ip, char * port, int id);

// Need to restart a replica with (id + 1) % REP_COUNT
void restartHandler(int signo) {
  printf("What up from the restart handler! Word. Signal: %d\n", signo);

  pid_t currentPID = 0;
  // fork
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // child sets new id, recreates connects, loops?
      id_number = id_number - 2; // This is ugly. For 3 reps, ids should be 2, 3, and 4
      id_number = (id_number + 1) % 3;
      id_number = id_number + 2;
      // TODO: The pid is not known to the voter
      createConnections(ip_address, port_number, id_number);
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

  if (argc < 6) {
    puts("Usage: art_pot <ip_address> <port> <position2d id> <read_in_fd> <write_out_fd>");
    return -1;
  }

  id_number = atoi(argv[3]);
  read_in_fd = atoi(argv[4]);
  write_out_fd = atoi(argv[5]);
  strncpy(ip_address, argv[1], sizeof(ip_address));
  strncpy(port_number, argv[2], sizeof(port_number));

  return 0;
}

// Should probably separate this out correctly
// Basically the init function
int createConnections(char * ip, char * port, int id) {
  InitTAS(DEFAULT_CPU, &cpu_speed);

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
  struct comm_header hdr;

  hdr.type = COMM_MOV_CMD;
  hdr.byte_count = 2 * sizeof(double);

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

    // Write move command
    write(write_out_fd, &hdr, sizeof(struct comm_header));

    write(write_out_fd, vel_cmd, hdr.byte_count);
  } else { // within distance epsilon. Give it up, man.
    vel_cmd[0] = 0.0;
    vel_cmd[1] = 0.0;
    write(write_out_fd, &hdr, sizeof(struct comm_header));

    write(write_out_fd, vel_cmd, hdr.byte_count);
  }
}

void requestWaypoints() {
  struct comm_header hdr;
  
  hdr.type = COMM_WAY_REQ;
  hdr.byte_count = 0;

  write(write_out_fd, &hdr, sizeof(struct comm_header));
}

void enterLoop() {
  void * update_id;
  //  int read_ret;
  int index;
  timestamp_t last;
  timestamp_t current;
  int read_ret;
  struct comm_header hdr;

  while(1) { // while something else.
    // This is how to read the ranges_count
    read_ret = read(read_in_fd, &hdr, sizeof(struct comm_header));
    if (read_ret > 0) {
      assert(read_ret == sizeof(struct comm_header));
      switch (hdr.type) {
      case COMM_RANGE_DATA:
	read_ret = read(read_in_fd, ranger_ranges, hdr.byte_count);
	ranger_count = read_ret / sizeof(double);      
	assert(read_ret == hdr.byte_count);

	// Calculates and sends the new command
	command();
	break;
      case COMM_POS_DATA:
	//	printf("Recieved Position Data\n");
	read_ret = read(read_in_fd, pos, hdr.byte_count);
	assert(read_ret == hdr.byte_count);
	break;
      case COMM_WAY_RES:
	//	printf("Recieved Waypoint Response\n");
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

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (createConnections(ip_address, port_number, id_number) < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  requestWaypoints();

  enterLoop();

  return 0;
}

