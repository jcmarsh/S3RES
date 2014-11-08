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

#define REP_COUNT 1

// Replica related data
struct replica_group repGroup;
struct replica replicas[REP_COUNT];

struct comm_range_pose_data range_pose_data_msg;
struct comm_mov_cmd mov_cmd_msg;

// TAS Stuff
cpu_speed_t cpu_speed;

int read_in_fd; // These are to your parent (Translator)
int write_out_fd;

timestamp_t last;

// FUNCTIONS!
int initBenchMarker();
int parseArgs(int argc, const char **argv);
void doOneUpdate();
void sendWaypoints();
void processOdom();
void processRanger();
void processCommand();

int initBenchMarker() {
  int scheduler;
  int p_offset = 0;

  InitTAS(DEFAULT_CPU, &cpu_speed, 0);

  scheduler = sched_getscheduler(0);
  printf("BenchMarker Scheduler: %d\n", scheduler);

  // Should only be a single replica
  initReplicas(&repGroup, replicas, REP_COUNT);
  forkSingleReplica(&repGroup, 0, "plumber");
  
  return 0;
}

int parseArgs(int argc, const char **argv) {
  int i;

  if (argc < 3) {
    puts("Usage: BenchMarker <read_in_fd> <write_out_fd>");
    return -1;
  }

  read_in_fd = atoi(argv[1]);
  write_out_fd = atoi(argv[2]);

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

  while(1) {
    doOneUpdate();
  }

  return 0;
}

void doOneUpdate() {
  int retval = 0;
  int index;
  struct comm_range_pose_data recv_msg_tran;
  struct comm_mov_cmd recv_msg_rep;

  // Message comes in from translator
  // Message goes out from replica
  // That is it. Except for those waypoint request / responses

  // Translator driven, check first. This call blocks.

  // TODO: handle waypoint responses
  retval = read(read_in_fd, &recv_msg_tran, sizeof(struct comm_range_pose_data));
  if (retval > 0) {
    // TODO: Check for erros
    memcpy(&range_pose_data_msg, &recv_msg_tran, sizeof(struct comm_range_pose_data));
    processRanger();      
  } else {
    perror("Bench: read should have worked, failed."); // EINTR?
  }

  // Second part of the cycle: response from replica
  // TODO: Handle waypoint requests
  retval = read(replicas[0].fd_outof_rep[0], &recv_msg_rep, sizeof(struct comm_mov_cmd));
  if (retval > 0) {
    mov_cmd_msg.vel_cmd[0] = recv_msg_rep.vel_cmd[0];
    mov_cmd_msg.vel_cmd[1] = recv_msg_rep.vel_cmd[1];
    processCommand();
  } else {
    perror("Bench: read should have worked, failed."); // EINTR?
  }
}


////////////////////////////////////////////////////////////////////////////////
// Process ranger data
void processRanger() {
  // msg data set by read in (TODO: Change?)

#ifdef _STATS_BENCH_ROUND_TRIP_
  last = generate_timestamp();
#endif // _STATS_BENCH_ROUND_TRIP_

  write(replicas[0].fd_into_rep[1], &range_pose_data_msg, sizeof(struct comm_range_pose_data));
}

////////////////////////////////////////////////////////////////////////////////
// Send commands to underlying position device
void processCommand() {
#ifdef _STATS_BENCH_ROUND_TRIP_
  timestamp_t current;
  long new_interrupts;

  current = generate_timestamp();
  // check against previous interrupt count

  printf("%lld\n", current - last);
#endif

  // data was set by read
  write(write_out_fd, &mov_cmd_msg, sizeof(struct comm_mov_cmd));
}
