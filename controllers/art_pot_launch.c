/*
 * For launching a replica Artificial Potential Threaded Driver 
 * Drivers are lanched by subscribing.
 */

#include <libplayerc/playerc.h>

int main(int argc, const char **argv) {
  int i;
  playerc_client_t *client;
  playerc_position2d_t *position2d_replica;

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
  position2d_replica = playerc_position2d_create(client, atoi(argv[3]));
  if (playerc_position2d_subscribe(position2d_replica, PLAYER_OPEN_MODE)) {
    return -1;
  }

  while(1) {
    // blah
  }

  // Shutdown
  playerc_position2d_unsubscribe(position2d_replica);
  playerc_position2d_destroy(position2d_replica);
  playerc_client_disconnect(client);
  playerc_client_destroy(client);

  return 0;
}

