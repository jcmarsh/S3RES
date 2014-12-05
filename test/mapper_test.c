/*
 * Should start, and wait for a Mapper program to connect (through unix domain sockets).
 * The just send dummy data to exercise the program so that Valgrind can catch errors.
 */

#include "../include/commtypes.h"
#include "../include/replicas.h"
#include "../include/fd_server.h"

struct replica rep;
char* controller_name = "Mapper";
struct typed_pipe pipes[PIPE_LIMIT];

// FD server
struct server_data sd;

int main(int argc, const char** argv) {
  // Setup fd server
  createFDS(&sd, "MapperTest");

  // Setup pipe type and direction
  // Ranger data in
  pipes[0].type = RANGE_POSE_DATA;
  pipes[0].fd_in = 42;
  pipes[0].fd_out = 0;
  // map updates out
  pipes[1].type = MAP_UPDATE;
  pipes[1].fd_in = 0;
  pipes[1].fd_out = 42;
  // Comm acks in
  pipes[2].type = COMM_ACK;
  pipes[2].fd_in = 42;
  pipes[2].fd_out = 0;

  initReplicas(&rep, 1, controller_name, 10);
  createPipes(&rep, 1, pipes, 3);
  // send new pipe through fd server (should have a request)
  acceptSendFDS(&sd, &(rep.pid), rep.rep_pipes, rep.pipe_count);

  // Should be connected now.

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

    // read the map update, should at least have the pose
    char buffer[1024];
    read(rep.vot_pipes[1].fd_in, buffer, sizeof(buffer));
    printf("Pose returned %d, %d\n", ((int *)buffer)[0], ((int *)buffer)[1]);

    // write the ack
    struct comm_ack sim_ack;
    sim_ack.padding = 0;
    write(rep.vot_pipes[2].fd_out, &sim_ack, sizeof(struct comm_ack));

    sleep(1);
  }
}