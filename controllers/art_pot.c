/*
 * Artificial Potential controller (instead of Threaded Driver)
 */

#include <libplayerc/playerc.h>

int main(int argc, const char **argv) {
  int i;
  playerc_client_t *client;
  playerc_position2d_t *position2d; // Check position, send velocity commands
  playerc_planner_t *planner; // Check for new position goals
  playerc_laser_t *laser; // laser sensor readings

  if (argc < 4) {
    puts("Usage: art_pot_launch <ip_address> <port> <position2d id>");
    return 0;
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

  while(1) { // while something else.
    // Controllers control.

  }

  // Shutdown
  playerc_laser_unsubscribe(laser);
  playerc_laser_destroy(laser);
  playerc_planner_unsubscribe(planner);
  playerc_planner_destroy(planner);
  playerc_position2d_unsubscribe(position2d);
  playerc_position2d_destroy(position2d);
  playerc_client_disconnect(client);
  playerc_client_destroy(client);

  return 0;
}

