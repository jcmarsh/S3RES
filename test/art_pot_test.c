// Test ArtPot

#include "../include/commtypes.h"
#include "../include/replicas.h"
#include "../include/fd_server.h"

struct replica rep;
char* controller_name = "ArtPot";
struct typed_pipe pipes[PIPE_LIMIT];

// FD server
struct server_data sd;

int main(int argc, const char** argv) {
  // Setup fd server
  //createFDS(&sd, controller_name);
  createFDS(&sd, "ArtPotTest");

  // Setup pipe type and direction
  // Ranger data in
  pipes[0].type = RANGE_POSE_DATA;
  pipes[0].fd_in = 42;
  pipes[0].fd_out = 0;
  // waypoint request out
  pipes[1].type = WAY_REQ;
  pipes[1].fd_in = 0;
  pipes[1].fd_out = 42;
  // waypoint response in
  pipes[2].type = WAY_RES;
  pipes[2].fd_in = 42;
  pipes[2].fd_out = 0;
  // move command out
  pipes[3].type = MOV_CMD;
  pipes[3].fd_in = 0;
  pipes[3].fd_out = 42;

  initReplicas(&rep, 1, controller_name);
  createPipes(&rep, 1, pipes, 4);
  // send new pipe through fd server (should have a request)
  acceptSendFDS(&sd, &(rep.pid), rep.rep_pipes, rep.pipe_count);

  // Should be connected now.

  // First need to read the waypoint request
  struct comm_way_req way_req;
  read(rep.vot_pipes[1].fd_in, &way_req, sizeof(way_req));
  printf("Got waypoint req\n");

  // Send response
  struct comm_way_res way_res;
  way_res.point[0] = 8.0;
  way_res.point[1] = 8.0;
  way_res.point[2] = 0.0;
  write(rep.vot_pipes[2].fd_out, &way_res, sizeof(way_res));

  int loops = 100;
  while (loops--) {
	// Create and send some ranger data
  	struct comm_range_pose_data sim_range_data;
  	sim_range_data.pose[0] = -2.0;
  	sim_range_data.pose[1] = -2.0;
  	sim_range_data.pose[2] = 1.0;
  	int i = 0;
  	for (i = 0; i < 16; i++) {
  		sim_range_data.ranges[i] = i * 1.5;
  	}
  	write(rep.vot_pipes[0].fd_out, &sim_range_data, sizeof(struct comm_range_pose_data));

	// read the command out
  	struct comm_mov_cmd mov_cmd;
	read(rep.vot_pipes[3].fd_in, &mov_cmd, sizeof(mov_cmd));
	printf("Move Command: %f, %f\n", mov_cmd.vel_cmd[0], mov_cmd.vel_cmd[1]);

	sleep(1);
  }
}