// Test AStar

#include "../include/commtypes.h"
#include "../include/replicas.h"
#include "../include/fd_server.h"

struct replica rep;
const char* controller_name = "AStar";
struct typed_pipe pipes[PIPE_LIMIT];

// FD server
struct server_data sd;

int main(int argc, const char** argv) {
  // Setup fd server
  createFDS(&sd, "AStarTest");

  // Setup pipe type and direction
  // Map Update data in
  pipes[0].type = MAP_UPDATE;
  pipes[0].fd_in = 42;
  pipes[0].fd_out = 0;
  // ack out
  pipes[1].type = COMM_ACK;
  pipes[1].fd_in = 0;
  pipes[1].fd_out = 42;
  // waypoint request in
  pipes[2].type = WAY_REQ;
  pipes[2].fd_in = 42;
  pipes[2].fd_out = 0;
  // waypoint resopnse out
  pipes[3].type = WAY_RES;
  pipes[3].fd_in = 0;
  pipes[3].fd_out = 42;

  initReplicas(&rep, 1, controller_name, 10);
  createPipes(&rep, 1, pipes, 4);
  // send new pipe through fd server (should have a request)
  acceptSendFDS(&sd, &(rep.pid), rep.rep_pipes, rep.pipe_count);

  // Should be connected now.

  int loops = 100;
  int c = 0;
  int i;
  while (loops--) {
  	// Send Map update
  	struct comm_map_update map_update;
  	map_update.pose_x = 4;
  	map_update.pose_y = 5;
  	map_update.obs_count = 5;
  	int obs_x[5];
  	int obs_y[5];
  	for (i = 0; i < 5; i++) {
  		obs_x[i] = c;
  		obs_y[i] = c;
  		c++;
  	}
  	map_update.obs_x = obs_x;
  	map_update.obs_y = obs_y;
  	commSendMapUpdate(rep.vot_pipes[0], &map_update);

  	// Get ack
  	struct comm_ack ack;
  	read(rep.vot_pipes[1].fd_in, &ack, sizeof(ack));
    printf("AStar acked\n");

  	// send waypoint request
  	commSendWaypointRequest(rep.vot_pipes[2]);

  	// get waypoint response
  	struct comm_way_res way_res;
  	read(rep.vot_pipes[3].fd_in, &way_res, sizeof(way_res));
    printf("Waypoint: %f, %f\n", way_res.point[0], way_res.point[1]);

  	sleep(1);
  }
}