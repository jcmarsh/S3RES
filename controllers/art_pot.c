/*
 * Artificial Potential controller (instead of Threaded Driver)
 */

#include <libplayerc/playerc.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "../include/taslimited.h"

// Configuration parameters
#define VEL_SCALE 1
#define DIST_EPSILON .1
#define GOAL_RADIUS 0
#define GOAL_EXTENT 1
#define GOAL_SCALE 1
#define OBST_RADIUS 0
#define OBST_EXTENT 1
#define OBST_SCALE .3

// player interfaces
playerc_client_t *client;
playerc_position2d_t *pos2d; // Check position, send velocity command
playerc_planner_t *planner; // Check for new position goals
playerc_ranger_t *ranger; // ranger sensor readings

// Controller state
bool active_goal;
double goal_x, goal_y, goal_a;

// Position
double pos_x, pos_y, pos_a;
int ranger_count;
double ranger_ranges[365]; // 365 comes from somewhere in Player as the max.

char ip_address[17];
char port_number[10];
int id_number;

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

  if (argc < 4) {
    puts("Usage: art_pot <ip_address> <port> <position2d id>");
    return -1;
  }

  id_number = atoi(argv[3]);
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

  // Create client and connect
  client = playerc_client_create(0, ip, atoi(port)); // I start at 6666
  if (0 != playerc_client_connect(client)) {
    return -1;
  }

  playerc_client_datamode(client, PLAYERC_DATAMODE_PUSH);
  
  // TODO: I can't imagine it is acceptable to use atoi() unchecked.
  // Subscribe to a redundant driver so that it will run!
  pos2d = playerc_position2d_create(client, id);
  if (playerc_position2d_subscribe(pos2d, PLAYER_OPEN_MODE)) {
    return -1;
  }

  planner = playerc_planner_create(client, id);
  if (playerc_planner_subscribe(planner, PLAYER_OPEN_MODE)) {
    return -1;
  }

  ranger = playerc_ranger_create(client, 2);
  if (playerc_ranger_subscribe(ranger, PLAYER_OPEN_MODE)) {
    return -1;
  }
  return 0;
}

void shutdownArtPot() {
  playerc_ranger_unsubscribe(ranger);
  playerc_ranger_destroy(ranger);
  playerc_planner_unsubscribe(planner);
  playerc_planner_destroy(planner);
  playerc_position2d_unsubscribe(pos2d);
  playerc_position2d_destroy(pos2d);
  playerc_client_disconnect(client);
  playerc_client_destroy(client);
}

void command() {
  double dist, theta, delta_x, delta_y, v, tao, obs_x, obs_y, vel, rot_vel;
  int total_factors, i;

  // Head towards the goal! odom_pose: 0-x, 1-y, 2-theta
  dist = sqrt(pow(goal_x - pos_x, 2)  + pow(goal_y - pos_y, 2));
  theta = atan2(goal_y - pos_y, goal_x - pos_x) - pos_a;

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
  
    vel = sqrt(pow(delta_x, 2) + pow(delta_y, 2));
    rot_vel = atan2(delta_y, delta_x);
    vel = VEL_SCALE * vel * (abs(M_PI - rot_vel) / M_PI);
    rot_vel = VEL_SCALE * rot_vel;

    // TODO: Should turn to the desired theta, goal_t
    playerc_position2d_set_cmd_vel(pos2d, vel, 0, rot_vel, 1);
  } else { // within distance epsilon. Give it up, man.
    playerc_position2d_set_cmd_vel(pos2d, 0, 0, 0, 1);
  }
}

void requestWaypoints() {
  playerc_planner_get_waypoints(planner);
  goal_x = planner->waypoints[0][0];
  goal_y = planner->waypoints[0][1];
  goal_a = planner->waypoints[0][2];
}

void enterLoop() {
  void * update_id;
  int index;

  while(1) { // while something else.
    update_id = playerc_client_read(client);

    // figure out type of update from read
    if (update_id == client->id) {         // client update?
      //      puts("CLIENT UPDATED");
    } else if (update_id == planner->info.id) { // planner? shouldn't be updating here.
      //      puts("PLANNER UPDATED");
    } else if (update_id == ranger->info.id) {   // ranger readings update
      //      puts("RANGER UPDATED");     
      pos_x = pos2d->px;
      pos_y = pos2d->py;
      pos_a = pos2d->pa;
      ranger_count = ranger->ranges_count;
      for (index = 0; index < ranger_count; index++) {
	ranger_ranges[index] = ranger->ranges[index];
      }
      // Calculates and sends the new command
      command(); 
    } else if (update_id == pos2d->info.id) {
      //      puts("POSITION2D UPDATED");
    } else if (update_id == NULL) {
      puts("ERROR: client read error.");
      //      exit(-1);
    } else {
      puts("UNKNOWN");
    }
      //      printf("\t update_id: %p\n", update_id);
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

  shutdownArtPot();
  return 0;
}

