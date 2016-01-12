// Micro benchmark for round trip communication with an Empty controller

#include <time.h>

#include "taslimited.h"
#include "commtypes.h"
#include "replicas.h"

struct replica rep;
const char* controller_name = "Empty";
struct vote_pipe pipes[PIPE_LIMIT];

// FD server
struct server_data sd;

timestamp_t last;

int main(int argc, const char** argv) {
  InitTAS(DEFAULT_CPU, 5);
  // Setup fd server
  createFDS(&sd, "Empty");

  // Setup pipe type and direction
  // Ranger data in
  pipes[0].rep_info = (char *) MESSAGE_T[RANGE_POSE_DATA];
  pipes[0].fd_in = 42;
  pipes[0].fd_out = 0;
  // Move Command Out
  pipes[1].rep_info = (char *) MESSAGE_T[MOV_CMD];
  pipes[1].fd_in = 0;
  pipes[1].fd_out = 42;

  initReplicas(&rep, 1, controller_name, 10);
  createPipes(&rep, 1, pipes, 2);
  // send new pipe through fd server (should have a request)
  acceptSendFDS(&sd, &(rep.pid), rep.rep_pipes, rep.pipe_count);

  // Should be connected now.
  int loops = 1000001;
  while (loops--) {
    // Create and send some ranger data
    struct comm_range_pose_data sim_range_data;
    struct comm_mov_cmd sim_mov_cmd;
    sim_range_data.pose[0] = -2.0;
    sim_range_data.pose[1] = -5.0;
    sim_range_data.pose[2] = 1.0;
    int i = 0;
    for (i = 0; i < 16; i++) {
      sim_range_data.ranges[i] = i * 1.5;
    }

    last = generate_timestamp();

    if (write(rep.vot_pipes[0].fd_out, &sim_range_data, sizeof(struct comm_range_pose_data)) != sizeof(struct comm_range_pose_data)) {
      perror("Write failed");
    }

    // read filtered data
    if (read(rep.vot_pipes[1].fd_in, &sim_mov_cmd, sizeof(sim_mov_cmd)) != sizeof(struct comm_mov_cmd)) {
      perror("Read failed");
    }
    
    timestamp_t current = generate_timestamp();

    printf("usec (%lf)\n", diff_time(current, last, CPU_MHZ));
  }
}
