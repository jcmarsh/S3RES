// Test filter

#include "../include/commtypes.h"
#include "../include/replicas.h"
#include "../include/fd_server.h"

struct replica rep;
char* controller_name = "Filter";
struct typed_pipe pipes[PIPE_LIMIT];

// FD server
struct server_data sd;

int main(int argc, const char** argv) {
  // Setup fd server
  createFDS(&sd, "FilterTest");

  // Setup pipe type and direction
  // Ranger data in
  pipes[0].type = RANGE_POSE_DATA;
  pipes[0].fd_in = 42;
  pipes[0].fd_out = 0;
  // Ranger out
  pipes[1].type = RANGE_POSE_DATA;
  pipes[1].fd_in = 0;
  pipes[1].fd_out = 42;
  // Ranger out again
  pipes[2].type = RANGE_POSE_DATA;
  pipes[2].fd_in = 0;
  pipes[2].fd_out = 42;

  initReplicas(&rep, 1, controller_name);
  createPipes(&rep, 1, pipes, 3);
  // send new pipe through fd server (should have a request)
  acceptSendFDS(&sd, &(rep.pid), rep.rep_pipes, rep.pipe_count);

  // Should be connected now.

  int loops = 100;
  while (loops--) {
	// Create and send some ranger data
  	struct comm_range_pose_data sim_range_data;
  	sim_range_data.pose[0] = -2.0;
  	sim_range_data.pose[1] = -5.0;
  	sim_range_data.pose[2] = 1.0;
  	int i = 0;
  	for (i = 0; i < 16; i++) {
  		sim_range_data.ranges[i] = i * 1.5;
  	}

  	write(rep.vot_pipes[0].fd_out, &sim_range_data, sizeof(struct comm_range_pose_data));

	// read filtered data
	read(rep.vot_pipes[1].fd_in, &sim_range_data, sizeof(sim_range_data));
	printf("Pose returned %f, %f\n", sim_range_data.pose[0], sim_range_data.pose[1]);

	// Read the other filtered data (meant for mapper
	read(rep.vot_pipes[2].fd_in, &sim_range_data, sizeof(sim_range_data));
	printf("Ranges returned %f, %f, %f ...\n", sim_range_data.ranges[0], sim_range_data.ranges[1], sim_range_data.ranges[2]);

	sleep(1);
  }
}