/*
 * Connects to a position2d device (number 1) at the specified IP.
 */

#include <unistd.h>
#include <libplayerc/playerc.h>
#include <math.h>

#define START_X -7.0
#define START_Y -7.0
#define GOAL_X   7.0
#define GOAL_Y   7.0
#define DIST_EPS 0.1

playerc_client_t *robot_client;
playerc_ranger_t *robot_ranger;
playerc_client_t *sim_client;
playerc_simulation_t *simulation;

double dist(double a_x, double a_y, double b_x, double b_y) {
  return sqrt(((a_x - b_x) * (a_x - b_x)) + ((a_y - b_y) * (a_y - b_y)));
}

int init(const char *ip_address) {
  // Create client for the sim commands and connect
  sim_client = playerc_client_create(0,  ip_address, 6665);
  if (0 != playerc_client_connect(sim_client)) {
    return -1;
  }

  // Create and subscribe to sim device (to reset position)
  simulation = playerc_simulation_create(sim_client, 0);
  if (playerc_simulation_subscribe(simulation, PLAYER_OPEN_MODE)) {
    return -1;
  }

  robot_client = playerc_client_create(0,  ip_address, 6666);
  if (0 != playerc_client_connect(robot_client)) {
    printf("Failed to connect to robot client.\n");
    return -1;
  }

  robot_ranger = playerc_ranger_create(robot_client, 1);
  if (playerc_ranger_subscribe(robot_ranger, PLAYER_OPEN_MODE)) {
    printf("Failed to subscribe to robot's ranger device.\n");
    return -1;
  }
}

// from: http://playerstage.sourceforge.net/doc/Player-svn/player/group__libplayerc__example.html
int main(int argc, const char **argv) {
  if (argc < 2) {
    printf("Usage: basic <ip_address>");
    return 0;
  } else {
    if (init(argv[1]) < 0) {
      printf("Failed to init logger.\n");
      return -1;
    }
  }

  // TODO: test, then remove
  double min_angle, max_angle, angular_res, min_range, max_range, range_res, frequency;
  if (playerc_ranger_get_config(robot_ranger, &min_angle, &max_angle, &angular_res, &min_range, &max_range, &range_res, &frequency)) {
    printf("Failed to get config for robot ranger.]n");
    return -1;
  }

  printf("Ranger fequency (%f) and stats:\n", frequency);
  printf("\tMin and max angles and resolution: %f - %f: %f\n", min_angle, max_angle, angular_res);
  printf("\tMin and max ranges and resolution: %f - %f: %f\n", min_range, max_range, range_res);
  // end todo

  int index = 0;
  double prev_time = robot_client->datatime;
  double prev_x = -999, prev_y = -999;

  while(1) {
    usleep(1000);
    // Calculate velocity and minimum distance from an obstacle.

    //long current_time = playerc_simulation_get_time(simulation, 0);
    double pos_x, pos_y, pos_a;
    playerc_simulation_get_pose2d(simulation, "hank", &pos_x, &pos_y, &pos_a);
    double current_time = robot_client->datatime;
    // printf("Pos at time %f is (%f, %f) - %f\n", current_time, pos_x, pos_y, pos_a);

    if (dist(pos_x, pos_y, START_X, START_Y) < DIST_EPS) {
      // nothing, too close to start
    } else if (dist(pos_x, pos_y, GOAL_X, GOAL_Y) < DIST_EPS + .8) { // Robot stops short
      prev_x = -999;
      prev_y = -999;
      // nothing, too close to end
    } else {
      // calc velocity
      double distance = -1;
      if (prev_x != -999) {
        distance = dist(prev_x, prev_y, pos_x, pos_y);
      } else {
	printf("Start of new run\n");
      }

      // obstacle distance
      // Read data
      playerc_client_read(robot_client);
      double min = 1000; // approximately infinite.
      for (index = 0; index < 16; index++) {
        if (robot_ranger->ranges[index] < min) {
          min = robot_ranger->ranges[index];
        }
      }

      //printf("Velocity = %f\tMin dist = %f\n", distance / (current_time - prev_time), min); 
      printf("(%f,\t%f)\n", distance / (current_time - prev_time), min); 

      prev_time = current_time;
      prev_x = pos_x;
      prev_y = pos_y;
    }
  }

  // Shutdown
  playerc_client_disconnect(robot_client);
  playerc_client_destroy(robot_client);
  playerc_client_disconnect(sim_client);
  playerc_client_destroy(sim_client);

  return 0;
}

