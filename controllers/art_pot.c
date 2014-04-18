/*
 * Artificial Potential controller (instead of Threaded Driver)
 */

#include <libplayerc/playerc.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include "../include/customtimer.h"

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
playerc_laser_t *laser; // laser sensor readings

// Controller state
bool active_goal;
double goal_x, goal_y, goal_a;
int id;

void restartHandler(int signo) {
  // Need to restart a replica with (id + 1) % REP_COUNT
  printf("Shit... this may work\n");
}

int setupArtPot(int argc, const char **argv) {
  int i;

  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }

  if (argc < 4) {
    puts("Usage: art_pot <ip_address> <port> <position2d id>");
    return -1;
  }

  id = atoi(argv[3]);

  // Create client and connect
  client = playerc_client_create(0, argv[1], atoi(argv[2])); // I start at 6666
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

  laser = playerc_laser_create(client, id);
  if (playerc_laser_subscribe(laser, PLAYER_OPEN_MODE)) {
    return -1;
  }

}

void shutdownArtPot() {
  playerc_laser_unsubscribe(laser);
  playerc_laser_destroy(laser);
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

#ifdef TIME_ART_POT
  struct timespec start;
  struct timespec end;

  clock_gettime(CLOCK_REALTIME, &start);
#endif

  // Head towards the goal! odom_pose: 0-x, 1-y, 2-theta
  dist = sqrt(pow(goal_x - pos2d->px, 2)  + pow(goal_y - pos2d->py, 2));
  theta = atan2(goal_y - pos2d->py, goal_x - pos2d->px) - pos2d->pa;

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
    for (i = 0; i < laser->scan_count; i++) {
      // figure out location of the obstacle...
      tao = (2 * M_PI * i) / laser->scan_count;
      obs_x = laser->ranges[i] * cos(tao);
      obs_y = laser->ranges[i] * sin(tao);
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

#ifdef TIME_ART_POT
  clock_gettime(CLOCK_REALTIME, &end);

  PRINT_MICRO("ArtPot", start, end);
#endif
}

int main(int argc, const char **argv) {
  void * update_id;

#ifdef TIME_FORK
  struct timespec now;

  clock_gettime(CLOCK_REALTIME, &now);
  PRINT_SINGLE("\tChild Start", now);
#endif

  if (setupArtPot(argc, argv) < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  // should loop until waypoints are received? Or does this block?
  playerc_planner_get_waypoints(planner);
  goal_x = planner->waypoints[0][0];
  goal_y = planner->waypoints[0][1];
  goal_a = planner->waypoints[0][2];

  while(1) { // while something else.
    update_id = playerc_client_read(client);

    // figure out type of update from read
    if (update_id == client->id) {         // client update?
      //      puts("CLIENT UPDATED");
    } else if (update_id == planner->info.id) { // planner? shouldn't be updating here.
      //      puts("PLANNER UPDATED");
    } else if (update_id == laser->info.id) {   // laser readings update
      //      puts("LASER UPDATED");     
      // TODO: Could wait for both position and laser to be fresh.
      // Calculates and sends the new command
      command(); 
    } else if (update_id == pos2d->info.id) {
      //      puts("POSITION2D UPDATED");
    } else if (update_id == NULL) {
      puts("ERROR");
      exit(-1);
    } else {
      puts("UNKNOWN");
    }
      
      //      printf("\t update_id: %p\n", update_id);
  }

  shutdownArtPot();
  return 0;
}

