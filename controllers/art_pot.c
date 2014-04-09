/*
 * Artificial Potential controller (instead of Threaded Driver)
 */

#include <libplayerc/playerc.h>
#include <stdbool.h>

// Configuration parameters
#define VEL_SCALE 1
#define DIST_EPSILON .1
#define GOAL_RADIUS 0
#define GOAL_EXTENT 1
#define GOAL_SCALE 1
#define OBSTACLE_RADIUS 0
#define OBSTACLE_EXTENT 1
#define OBSTACLE_SCALE .3

// player interfaces
playerc_client_t *client;
playerc_position2d_t *position2d; // Check position, send velocity command
playerc_planner_t *planner; // Check for new position goals
playerc_laser_t *laser; // laser sensor readings

// Controller state
bool active_goal;
double goal_x, goal_y, goal_t;
int cmd_state;

int setupArtPot(int argc, const char **argv) {
 int i;

  if (argc < 4) {
    puts("Usage: art_pot_launch <ip_address> <port> <position2d id>");
    return -1;
  }

  // Create client and connect
  client = playerc_client_create(0, argv[1], atoi(argv[2])); // I start at 6666
  if (0 != playerc_client_connect(client)) {
    return -1;
  }
  
  // TODO: I can't imagine it is acceptable to use atoi() unchecked.
  // Subscribe to a redundant driver so that it will run!
  position2d = playerc_position2d_create(client, atoi(argv[3]));
  if (playerc_position2d_subscribe(position2d, PLAYER_OPEN_MODE)) {
    return -1;
  }

  planner = playerc_planner_create(client, atoi(argv[3]));
  if (playerc_planner_subscribe(planner, PLAYER_OPEN_MODE)) {
    return -1;
  }

  laser = playerc_laser_create(client, atoi(argv[3]));
  if (playerc_laser_subscribe(laser, PLAYER_OPEN_MODE)) {
    return -1;
  }

}

void shutdownArtPot() {
  playerc_laser_unsubscribe(laser);
  playerc_laser_destroy(laser);
  playerc_planner_unsubscribe(planner);
  playerc_planner_destroy(planner);
  playerc_position2d_unsubscribe(position2d);
  playerc_position2d_destroy(position2d);
  playerc_client_disconnect(client);
  playerc_client_destroy(client);
}

int main(int argc, const char **argv) {
  void * update_id;

  if (setupArtPot(argc, argv) < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  // should loop until waypoints are received
  playerc_planner_get_waypoints(planner);

  while(1) { // while something else.
    update_id = playerc_client_read(client);

    // figure out type of update from read
    if (update_id == client->id) {         // client update?
      puts("CLIENT UPDATED");
    } else if (update_id == planner->info.id) { // planner? shouldn't be updating here.
      puts("PLANNER UPDATED");
    } else if (update_id == laser->info.id) {   // laser readings update
      puts("LASER UPDATED");
    } else {
      puts("UNHANDLED");
      printf("\t update_id: %p\n", update_id);
    }

    // calculate new command
    // publish command
  }

  shutdownArtPot();

  return 0;
}

