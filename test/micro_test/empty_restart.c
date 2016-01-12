// Test restart times for the Empty controller

#include <time.h>

#include "taslimited.h"
#include "controller.h"
#include "replicas.h"

// Replica related data
struct replica replicas[2];

// TAS Stuff
int voter_priority = 5;

// FD server
struct server_data sd;

int rep_count = DMR;
const char* controller_name = "Empty";

// pipes to external components (not replicas)
int pipe_count = 2;
struct vote_pipe ext_pipes[2];

int main(int argc, const char **argv) {
  InitTAS(DEFAULT_CPU, voter_priority);

  // Setup fd server
  createFDS(&sd, controller_name);

  // Setup pipe type and direction
  // Ranger data in
  ext_pipes[0].rep_info = (char *) MESSAGE_T[RANGE_POSE_DATA];
  ext_pipes[0].fd_in = 42;
  ext_pipes[0].fd_out = 0;
  // Move Command Out
  ext_pipes[1].rep_info = (char *) MESSAGE_T[MOV_CMD];
  ext_pipes[1].fd_in = 0;
  ext_pipes[1].fd_out = 42;

  startReplicas(replicas, rep_count, &sd, controller_name, ext_pipes, 2, voter_priority - 5);

  int loops = 1000;
  while(loops--) {
    // pick one at random
    int restartee = rand() % 2;

    int restarter = (restartee + (rep_count - 1)) % rep_count;
    //timestamp_t curr_time = generate_timestamp();
    //union sigval time_value;
    //time_value.sival_ptr = (void *)curr_time;
    //int retval = sigqueue(replicas[restarter].pid, SDC_SIM_SIGNAL, time_value);

    // kill it
    kill(replicas[restartee].pid, SIGKILL);

    // restart it
    timestamp_t last = generate_timestamp();
    restartReplica(replicas, rep_count, &sd, ext_pipes, restarter, restartee, voter_priority - 5);
    timestamp_t current = generate_timestamp();
    printf("usec (%lf)\n", diff_time(current, last, CPU_MHZ));
    
    usleep(100000);
  }

  int i;
  for (i = 0; i < rep_count; i++) {
    kill(replicas[i].pid, SIGKILL);
  }

  return 0;
}
