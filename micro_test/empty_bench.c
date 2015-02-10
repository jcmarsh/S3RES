// Test filter

#include <time.h>

#include "../include/taslimited.h"
#include "../include/commtypes.h"
#include "../include/replicas.h"
#include "../include/fd_server.h"

struct replica rep;
char* controller_name = "Empty";
struct typed_pipe pipes[PIPE_LIMIT];

// FD server
struct server_data sd;

cpu_speed_t cpu_speed;

timestamp_t last;

int main(int argc, const char** argv) {
  InitTAS(DEFAULT_CPU, &cpu_speed, 5);
  // Setup fd server
  createFDS(&sd, "EmptyTest");

  // Setup pipe type and direction
  // Ranger data in
  pipes[0].type = RANGE_POSE_DATA;
  pipes[0].fd_in = 42;
  pipes[0].fd_out = 0;
  // Move Command Out
  pipes[1].type = MOV_CMD;
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
    sim_range_data.pose[0] = -2.0;
    sim_range_data.pose[1] = -5.0;
    sim_range_data.pose[2] = 1.0;
    int i = 0;
    for (i = 0; i < 16; i++) {
      sim_range_data.ranges[i] = i * 1.5;
    }

    last = generate_timestamp();

    write(rep.vot_pipes[0].fd_out, &sim_range_data, sizeof(struct comm_range_pose_data));

    // read filtered data
    read(rep.vot_pipes[1].fd_in, &sim_range_data, sizeof(sim_range_data));
    
    timestamp_t current = generate_timestamp();
    // check against previous interrupt count

    printf("(%lld)\n", current - last);

    //    usleep(10000);
  }
}
