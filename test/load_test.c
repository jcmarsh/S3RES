// Test Empty

#include "test.h"

const char* controller_name = "Load";
struct typed_pipe t_pipes[PIPE_LIMIT];
//const char* controller_name = "VoterD";

// FD server
struct server_data sd;

int main(int argc, const char** argv) {
  // Setup fd server
  createFDS(&sd, controller_name);
  //createFDS(&sd, "EmptyTest");

  // Setup pipe type and direction
  pipes[0].rep_info = (char *) MESSAGE_T[MSG_BUFFER];
  pipes[0].fd_in = 0;
  pipes[0].fd_out = 42;

  initReplicas(&rep, 1, controller_name, 10);
  createPipes(&rep, 1, pipes, 1);
  // send new pipe through fd server (should have a request)
  acceptSendFDS(&sd, &(rep.pid), rep.rep_pipes, rep.pipe_count);

  convertVoteToTyped(rep.vot_pipes, 1, t_pipes);

  while (1) {
    // check for command out or a new waypoint request
    int retval = 0;

    struct timeval select_timeout;
    fd_set select_set;

    // See if any of the read pipes have anything
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    FD_ZERO(&select_set);
    FD_SET(rep.vot_pipes[0].fd_in, &select_set);

    retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);

    if (retval > 0) {
      if (FD_ISSET(rep.vot_pipes[0].fd_in, &select_set)) {
	struct comm_msg_buffer msg;
	commRecvMsgBuffer(&(t_pipes[0]), &msg);
	printf("LOGGED MSG: %s", msg.message);
	free(msg.message);
      }
    }
    usleep(1000);
  }
}
