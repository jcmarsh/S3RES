// Test Empty

#include "test.h"

//const char* controller_name = "Empty";
const char* controller_name = "VoterD";

// FD server
struct server_data sd;

int main(int argc, const char** argv) {
  // Setup fd server
  //createFDS(&sd, controller_name);
  createFDS(&sd, "EmptyTest");

  // Setup pipe type and direction
  // Ranger data in
  pipes[0].rep_info = (char *) MESSAGE_T[RANGE_POSE_DATA];
  pipes[0].fd_in = 42;
  pipes[0].fd_out = 0;
  // move command out
  pipes[1].rep_info = (char *) MESSAGE_T[MOV_CMD];
  pipes[1].fd_in = 0;
  pipes[1].fd_out = 42;

  initReplicas(&rep, 1, controller_name, 10);
  createPipes(&rep, 1, pipes, 2);
  // send new pipe through fd server (should have a request)
  acceptSendFDS(&sd, &(rep.pid), rep.rep_pipes, rep.pipe_count);
  // Should be connected now.

  while (1) {
	// Create and send some ranger data
  	struct comm_range_pose_data sim_range_data;
  	sim_range_data.pose[0] = -2.0;
  	sim_range_data.pose[1] = -2.0;
  	sim_range_data.pose[2] = 1.0;
  	int i = 0;
  	for (i = 0; i < 16; i++) {
  		sim_range_data.ranges[i] = i * 1.5;
  	}
    //printf("Writing data to pipes\n");
  	write(rep.vot_pipes[0].fd_out, &sim_range_data, sizeof(struct comm_range_pose_data));

    // check for command out or a new waypoint request
    int retval = 0;

    struct timeval select_timeout;
    fd_set select_set;

    // See if any of the read pipes have anything
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    FD_ZERO(&select_set);
    FD_SET(rep.vot_pipes[1].fd_in, &select_set);

    retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);

    if (retval > 0) {
      if (FD_ISSET(rep.vot_pipes[1].fd_in, &select_set)) {
        // read the command out
        struct comm_mov_cmd mov_cmd;
        read(rep.vot_pipes[1].fd_in, &mov_cmd, sizeof(mov_cmd));
        //printf("Move Command: %f, %f\n", mov_cmd.vel_cmd[0], mov_cmd.vel_cmd[1]);
      }
    }

  	usleep(1000);
  }
}
