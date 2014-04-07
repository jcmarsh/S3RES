/*
 * Connects to a position2d device (number 1) at the specified IP.
 */

#include <libplayerc/playerc.h>

int main(int argc, const char **argv) {
  int i;
  playerc_client_t *client;
  playerc_position2d_t *position2d;

  if (argc < 2) {
    puts("Usage: basic <ip_address>");
    return 0;
  }

  // Create client and connect
  client = playerc_client_create(0, argv[1], 6666); // I start at 6666
  if (0 != playerc_client_connect(client)) {
    return -1;
  }

  // Create and subscribe to position2d device
  position2d = playerc_position2d_create(client, 1);
  if (playerc_position2d_subscribe(position2d, PLAYER_OPEN_MODE)) {
    return -1;
  }

  // Make robot move
  //  playerc_position2d_enable(position2d, 1);
  //  playerc_position2d_set_cmd_vel(position2d, 0.3, 0.2, 0.1, 1);
  playerc_position2d_set_cmd_pose(position2d, 7.0, 7.0, 0.0, 1);

  while(1) {
    // blah
  }

  // Shutdown
  playerc_position2d_unsubscribe(position2d);
  playerc_position2d_destroy(position2d);
  playerc_client_disconnect(client);
  playerc_client_destroy(client);

  return 0;
}


