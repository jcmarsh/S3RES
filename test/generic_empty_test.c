// Test Empty

#include "test.h"

const char* controller_name = "Empty";
//const char* controller_name = "VoterD";

// FD server
struct server_data sd;

int main(int argc, const char** argv) {
  // Setup fd server
  createFDS(&sd, controller_name);
  //createFDS(&sd, "EmptyTest");

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

  char buffer[4096];
  char fake_msg[1024] = {1};
  while (1) {
    // Create and send some ranger data
    //printf("Writing data to pipes\n");
    //write(rep.vot_pipes[0].fd_out, "Check it, did that work?", sizeof("Check it, did that work?"));
    write(rep.vot_pipes[0].fd_out, fake_msg, sizeof(fake_msg));

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
        int read_ret = read(rep.vot_pipes[1].fd_in, buffer, sizeof(buffer));
        // printf("Generic Empty returned: %d : %s\n", read_ret, buffer);
        printf("Generic Empty returned: %d\n", read_ret);
      }
    }

  	usleep(100000);
  }
}
