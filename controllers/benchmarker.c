/*
 * Interposes between the translator and whatever we are testing!
 *
 * James Marshall
 */

#include "commtypes.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "bench_config.h"
#include "taslimited.h"
#include "replicas.h"

// Replica related data
struct replica replica;

struct comm_range_pose_data range_pose_data_msg;
struct comm_mov_cmd mov_cmd_msg;

int seq_count = -1;

// TAS Stuff
int priority;
int pinned_cpu;

 // These are to your parent (Translator)
struct typed_pipe trans_pipes[2];

// FUNCTIONS!
int initBenchMarker();
int parseArgs(int argc, const char **argv);
void enterLoop();

int initBenchMarker() {
  int scheduler;

  InitTAS(DEFAULT_CPU, priority);

  scheduler = sched_getscheduler(0);

  // Should only be a single replica
  struct replica* r_p = (struct replica *) &replica;
  initReplicas(r_p, 1, "plumber", 10);

  struct vote_pipe new_pipes[2];
  convertTypedToVote(trans_pipes, 2, new_pipes);
  createPipes(r_p, 1, new_pipes, 2);
  
  struct typed_pipe pipes[2];
  convertVoteToTyped(r_p->rep_pipes, 2, pipes);

  char *argv[2];
  argv[0] = serializePipe(pipes[0]);
  argv[1] = serializePipe(pipes[1]);
  debug_print("Args from bench: %s: %s %d %d\n", argv[0], MESSAGE_T[pipes[0].type], pipes[0].fd_in, pipes[0].fd_out);
  debug_print("Args from bench: %s: %s %d %d\n", argv[1], MESSAGE_T[pipes[1].type], pipes[1].fd_in, pipes[1].fd_out);

  forkReplicas(r_p, 1, 2, argv);
  
  return 0;
}

int parseArgs(int argc, const char **argv) {
  // pipe_count should always be 2
  if (argc < 5) {
    puts("Usage: BenchMarker <priority> <pipe_num> <read_in_fd> <write_out_fd>");
    int i;
    for (i = 0; i < argc; i++) {
      printf("\t Arg %d: %s\n", i, argv[i]);
    }
    return -1;
  }

  priority = atoi(argv[1]);
  int pipe_count = 2;
  replica.vot_pipes = (struct vote_pipe *) malloc(sizeof(struct vote_pipe) * pipe_count);
  replica.voter_rep_in_copy = (int *) malloc(sizeof(int) * pipe_count);
  replica.rep_pipes = (struct vote_pipe *) malloc(sizeof(struct vote_pipe) * pipe_count);

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

  sleep(2);

  int retval, i = 50;
  while (i > 0) {
    retval = read(trans_pipes[0].fd_in, &range_pose_data_msg, sizeof(struct comm_range_pose_data));
    //printf("benchmarker pre-read %d bytes\n", retval);
    i--;
  }
  //printf("benchmarker moving on to main loop.\n");

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

  while (1) {
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
	if (retval == sizeof(struct comm_range_pose_data)) {
	  if (seq_count + 1 != range_pose_data_msg.seq_count) {
	    fprintf(stderr, "BENCH SEQ ERROR: %d - %d\n", seq_count + 1, range_pose_data_msg.seq_count);
	  }
	  seq_count = range_pose_data_msg.seq_count;
	  if (waiting_response) {
	    debug_print("ERROR, sending data but still waiting on previous response.\n");
#ifdef TIME_FULL_BENCH
	    timestamp_t toss = generate_timestamp();
	    printf("Error time elapsed (usec): %lf\n", diff_time(toss, last, CPU_MHZ));
	    last = toss;
#endif
	  } else { // Data is dropped if PINT hasn't yet responded.
	    waiting_response = true;

#ifdef TIME_FULL_BENCH
	    last = generate_timestamp();
#endif

	    if (write(replica.vot_pipes[0].fd_out, &range_pose_data_msg, sizeof(struct comm_range_pose_data)) != sizeof(struct comm_range_pose_data)) {
	      perror("BenchMarker failed range data write");
	    }
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
	retval = read(replica.vot_pipes[1].fd_in, &mov_cmd_msg, sizeof(struct comm_mov_cmd));
	if (retval == sizeof(struct comm_mov_cmd)) {
	  if (seq_count != mov_cmd_msg.seq_count) {
	    fprintf(stderr, "ERROR BENCH (MOV_CMD) SEQ: %d - %d\n", seq_count, mov_cmd_msg.seq_count);
	  }
	  waiting_response = false;

#ifdef TIME_FULL_BENCH
	  timestamp_t current = generate_timestamp();
	  printf("usec (%lf)\n", diff_time(current, last, CPU_MHZ));
#endif

	  // data was set by read in enterLoop
	  if (write(trans_pipes[1].fd_out, &mov_cmd_msg, sizeof(struct comm_mov_cmd)) != sizeof(struct comm_mov_cmd)) {
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
