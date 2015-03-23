/*
 * Interposes between the translator and whatever we are testing!
 *
 * James Marshall
 */

#include "../include/commtypes.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/scheduler.h"
#include "../include/bench_config.h"
#include "../include/taslimited.h"
#include "../include/replicas.h"

// Replica related data
struct replica replica;

struct comm_range_pose_data range_pose_data_msg;
struct comm_mov_cmd mov_cmd_msg;

// TAS Stuff
int priority;

 // These are to your parent (Translator)
struct typed_pipe trans_pipes[2];

// FUNCTIONS!
int initBenchMarker();
int parseArgs(int argc, const char **argv);
void enterLoop();

int initBenchMarker() {
  int scheduler;

  //InitTAS(DEFAULT_CPU, priority);
  InitTAS(0, priority);

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
  // pipe_count should always be 2
  if (argc < 5) {
    puts("Usage: BenchMarker <priority> <pipe_num> <read_in_fd> <write_out_fd>");
    return -1;
  }

  deserializePipe(argv[3], &trans_pipes[0]);
  deserializePipe(argv[4], &trans_pipes[1]);

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

  #ifdef TIME_FULL_BENCH
    timestamp_t last;
  #endif

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
        retval = TEMP_FAILURE_RETRY(read(trans_pipes[0].fd_in, &range_pose_data_msg, sizeof(struct comm_range_pose_data)));
        if (retval == sizeof(struct comm_range_pose_data)) {
          if (waiting_response) {
            printf("ERROR, sending data but still waiting on previous response.\n");
            #ifdef TIME_FULL_BENCH
              timestamp_t toss = generate_timestamp();
              printf("Error time elapsed (%lld)\n", toss - last);
            #endif
          }
          waiting_response = true;

          #ifdef TIME_FULL_BENCH
            last = generate_timestamp(); // TODO: Not sure about this
          #endif

	  if (TEMP_FAILURE_RETRY(write(replica.vot_pipes[0].fd_out, &range_pose_data_msg, sizeof(struct comm_range_pose_data)) != sizeof(struct comm_range_pose_data))) {
            perror("BenchMarker failed range data write");
          }
        } else if (retval > 0) {
          printf("Bench pipe 0 read did no match expected size.\n");
        } else if (retval < 0) {
          perror("Bench - pipe 0 read problems");
        } else {
          perror("Bench retval == 0 on pipe 0");
        }
      }

      if (FD_ISSET(replica.vot_pipes[1].fd_in, &select_set)) {
        // Second part of the cycle: response from replica
        retval = TEMP_FAILURE_RETRY(read(replica.vot_pipes[1].fd_in, &mov_cmd_msg, sizeof(struct comm_mov_cmd)));
        if (retval == sizeof(struct comm_mov_cmd)) {
          waiting_response = false;

          #ifdef TIME_FULL_BENCH
            timestamp_t current = generate_timestamp();
            printf("(%lld)\n", current - last);
          #endif

          // data was set by read in enterLoop
	  if (TEMP_FAILURE_RETRY(write(trans_pipes[1].fd_out, &mov_cmd_msg, sizeof(struct comm_mov_cmd)) != sizeof(struct comm_mov_cmd))) {
            perror("Bencmarker failed mov_cmd write");
          }
        } else if (retval > 0) {
          printf("Bench pipe 1 read did no match expected size.\n");
        } else if (retval < 0) {
          perror("Bench - pipe 1 read problems");
        } else {
          perror("Bench retval == 0 on pipe 1");
        }
      }
    }
  }
}
