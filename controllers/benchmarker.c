/*
 * Interposes between the translator and whatever we are testing!
 *
 * James Marshall
 */

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>

#include "../include/taslimited.h"
#include "../include/statstime.h"
#include "../include/replicas.h"
#include "../include/commtypes.h"

// Replica related data
struct replica replica;

struct comm_range_pose_data range_pose_data_msg;
struct comm_mov_cmd mov_cmd_msg;

// TAS Stuff
cpu_speed_t cpu_speed;
int priority;

 // These are to your parent (Translator)
struct typed_pipe trans_pipes[2];

timestamp_t last;

// FUNCTIONS!
int initBenchMarker();
int parseArgs(int argc, const char **argv);
void enterLoop();
void sendWaypoints();
void processOdom();
void processRanger();
void processCommand();

int initBenchMarker() {
  int scheduler;
  int p_offset = 0;

  InitTAS(DEFAULT_CPU, &cpu_speed, priority);

  scheduler = sched_getscheduler(0);

  // Should only be a single replica
  struct replica* r_p = (struct replica *) &replica;
  initReplicas(r_p, 1, "plumber", 10);
  createPipesSpecial(r_p, 1, trans_pipes, 2);
  forkReplicasSpecial(r_p, 1);
  
  return 0;
}

int parseArgs(int argc, const char **argv) {
  priority = atoi(argv[1]);
  if (argc < 4) {
    puts("Usage: BenchMarker <read_in_fd> <write_out_fd>");
    return -1;
  }

  deserializePipe(argv[2], &trans_pipes[0]);
  deserializePipe(argv[3], &trans_pipes[1]);

  return 0;
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initBenchMarker() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  enterLoop();

  return 0;
}

void enterLoop() {
  struct timeval select_timeout;
  fd_set select_set;
  bool waiting_response = false;

  while(1) {
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    FD_ZERO(&select_set);
    FD_SET(trans_pipes[0].fd_in, &select_set);
    FD_SET(replica.vot_pipes[1].fd_in, &select_set);

    // Message comes in from translator
    // Message goes out from replica
    int retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    if (retval > 0) {
      if (FD_ISSET(trans_pipes[0].fd_in, &select_set)) {
        retval = read(trans_pipes[0].fd_in, &range_pose_data_msg, sizeof(struct comm_range_pose_data));
        if (retval > 0) {
          // TODO: Check for erros
          if (waiting_response) {
            printf("ERROR, sending data but still waiting on previous response.\n");
          }
          waiting_response = true;

          processRanger();      
        } else {
          perror("Bench: read should have worked, failed."); // EINTR?
        }
      }

      if (FD_ISSET(replica.vot_pipes[1].fd_in, &select_set)) {
        // Second part of the cycle: response from replica
        retval = read(replica.vot_pipes[1].fd_in, &mov_cmd_msg, sizeof(struct comm_mov_cmd));
        if (retval > 0) {
          waiting_response = false;

          processCommand();
        } else {
          perror("Bench: read should have worked, failed."); // EINTR?
        }
      }
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
// Process ranger data
void processRanger() {
  // msg data set by read in (TODO: Change?)

#ifdef _STATS_BENCH_ROUND_TRIP_
  last = generate_timestamp();
#endif // _STATS_BENCH_ROUND_TRIP_

  write(replica.vot_pipes[0].fd_out, &range_pose_data_msg, sizeof(struct comm_range_pose_data));
}

////////////////////////////////////////////////////////////////////////////////
// Send commands to underlying position device
void processCommand() {
#ifdef _STATS_BENCH_ROUND_TRIP_
  timestamp_t current = generate_timestamp();
  
  printf("%lld\n", current - last);
#endif

  // data was set by read
  write(trans_pipes[1].fd_out, &mov_cmd_msg, sizeof(struct comm_mov_cmd));
}
