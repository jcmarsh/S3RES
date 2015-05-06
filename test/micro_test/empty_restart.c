// Test Empty

#include <time.h>

#include "../include/controller.h"
#include "../include/replicas.h"
#include "../include/fd_server.h"

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
struct typed_pipe ext_pipes[2];

int main(int argc, const char **argv) {
  InitTAS(DEFAULT_CPU, voter_priority);

  // Setup fd server
  createFDS(&sd, controller_name);
  startReplicas();

  int loops = 1000;
  while(loops--) {
    // pick one at random
    int restartee = rand() % 2;

    //int restarter = (restartee + (rep_count - 1)) % rep_count;
    //timestamp_t curr_time = generate_timestamp();
    //union sigval time_value;
    //time_value.sival_ptr = (void *)curr_time;
    //int retval = sigqueue(replicas[restarter].pid, SDC_SIM_SIGNAL, time_value);

    // kill it
    kill(replicas[restartee].pid, SIGKILL);

    // restart it
    timestamp_t last = generate_timestamp();
    restartReplica(restartee);
    timestamp_t current = generate_timestamp();
    printf("(%lld)\n", current - last);
    
    usleep(100000);
  }

  while(1) {
    // restart it
    timestamp_t last = generate_timestamp();
    //restartReplica(restartee);
    timestamp_t current = generate_timestamp();
    printf("(%lld)\n", current - last);
    
    usleep(100000);
  }

  return 0;
}
