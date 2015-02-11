// Test filter

#include <time.h>

#include "../include/taslimited.h"
#include "../include/commtypes.h"
#include "../include/replicas.h"
#include "../include/fd_server.h"

// Replica related data
struct replica replicas[2];

// TAS Stuff
cpu_speed_t cpu_speed;
int voter_priority = 5;

// FD server
struct server_data sd;

replication_t rep_type = DMR;
int rep_count = rep_type;
char* controller_name = "Empty";

// pipes to external components (not replicas)
int pipe_count = 2;
struct typed_pipe ext_pipes[2];

void cleanupReplica(int rep_index) {
  // cleanup replica data structure
  for (int i = 0; i < replicas[rep_index].pipe_count; i++) {
    if (replicas[rep_index].vot_pipes[i].fd_in > 0) {
      close(replicas[rep_index].vot_pipes[i].fd_in);
    }
    if (replicas[rep_index].vot_pipes[i].fd_out > 0) {
      close(replicas[rep_index].vot_pipes[i].fd_out);
    }
    if (replicas[rep_index].rep_pipes[i].fd_in > 0) {
      close(replicas[rep_index].rep_pipes[i].fd_in);
    }
    if (replicas[rep_index].rep_pipes[i].fd_out > 0) {
      close(replicas[rep_index].rep_pipes[i].fd_out);
    }
  }

  return;
}

void startReplicas(void) {
  initReplicas(replicas, rep_count, controller_name, voter_priority + 5);
  createPipes(replicas, rep_count, ext_pipes, pipe_count);
  forkReplicas(replicas, rep_count);
  for (int i = 0; i < rep_count; i++) {
    if (acceptSendFDS(&sd, &(replicas[i].pid), replicas[i].rep_pipes, replicas[i].pipe_count) < 0) {
      printf("EmptyRestart acceptSendFDS call failed\n");
      exit(-1);
    }
  }
}

void restartReplica(int restartee) {
  int restarter = (restartee + (rep_count - 1)) % rep_count;
  cleanupReplica(restartee);

  int retval = kill(replicas[restarter].pid, SIGUSR1);
  if (retval < 0) {
    perror("EmptyRestart Signal Problem");
  }

  // re-init failed rep, create pipes
  initReplicas(&(replicas[restartee]), 1, controller_name, voter_priority + 5);
  createPipes(&(replicas[restartee]), 1, ext_pipes, pipe_count);
  // send new pipe through fd server (should have a request)
  acceptSendFDS(&sd, &(replicas[restartee].pid), replicas[restartee].rep_pipes, replicas[restartee].pipe_count);
}

timestamp_t last;
int main(int argc, const char **argv) {
  InitTAS(DEFAULT_CPU, &cpu_speed, voter_priority);

  // Setup fd server
  createFDS(&sd, controller_name);
  startReplicas();

  int loops = 1001;
  while(loops--) {
    // pick one at random
    int restartee = rand() % 2;

    // kill it
    kill(replicas[restartee].pid, SIGKILL);

    // start time
    last = generate_timestamp();

    // restart it
    restartReplica(restartee);
    
    timestamp_t current = generate_timestamp();
    printf("(%lld)\n", current - last);
    // show time

    usleep(100000);
  }

  return 0;
}
