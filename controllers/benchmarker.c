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

// TAS Stuff
cpu_speed_t cpu_speed;

int read_in_fd; // These are to your parent (Translator)
int write_out_fd;

timestamp_t last;

struct comm_message range_pose_data_msg;
struct comm_message way_req_msg;
struct comm_message way_res_msg;
struct comm_message mov_cmd_msg;

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

  way_req_msg.type = COMM_WAY_REQ;
  way_res_msg.type = COMM_WAY_RES;
  mov_cmd_msg.type = COMM_MOV_CMD;
  range_pose_data_msg.type = COMM_RANGE_POSE_DATA;  

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
  struct comm_message recv_msg;

  // Message comes in from translator
  // Message goes out from replica
  // That is it. Except for those waypoint request / responses

  // Translator driven, check first. This call blocks.
  retval = read(read_in_fd, &recv_msg, sizeof(struct comm_message));
  if (retval > 0) {
    switch (recv_msg.type) {
    case COMM_RANGE_POSE_DATA:
      memcpy(&range_pose_data_msg, &recv_msg, sizeof(struct comm_message));
      processRanger();      
      break;
    case COMM_WAY_RES:
      memcpy(&way_res_msg, &recv_msg, sizeof(struct comm_message));
      sendWaypoints();
      break;
    default:
      printf("ERROR: BenchMarker can't handle comm type: %d\n", recv_msg.type);
    }
  } else {
    perror("Bench: read should have worked, failed."); // EINTR?
  }

  // Second part of the cycle: response from replica
  retval = read(replicas[0].fd_outof_rep[0], &recv_msg, sizeof(struct comm_message));
  if (retval > 0) {
    switch (recv_msg.type) {
    case COMM_WAY_REQ:
      // To translator
      memcpy(&way_req_msg, &recv_msg, sizeof(struct comm_message));
      write(write_out_fd, &way_req_msg, sizeof(struct comm_message));
      break;
    case COMM_MOV_CMD:
      mov_cmd_msg.data.m_cmd.vel_cmd[0] = recv_msg.data.m_cmd.vel_cmd[0];
      mov_cmd_msg.data.m_cmd.vel_cmd[1] = recv_msg.data.m_cmd.vel_cmd[1];
      processCommand();
      break;
    default:
      printf("ERROR: BenchMarker can't handle comm type: %d\n", recv_msg.type);
    }
  } else {
    perror("Bench: read should have worked, failed."); // EINTR?
  }
}

void sendWaypoints() {
  // data set by read

  write(replicas[0].fd_into_rep[1], &way_res_msg, sizeof(struct comm_message));
}

////////////////////////////////////////////////////////////////////////////////
// Process ranger data
void processRanger() {
  // msg data set by read in (TODO: Change?)

#ifdef _STATS_BENCH_ROUND_TRIP_
  last = generate_timestamp();
#endif // _STATS_BENCH_ROUND_TRIP_

  write(replicas[0].fd_into_rep[1], &range_pose_data_msg, sizeof(struct comm_message));
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
  write(write_out_fd, &mov_cmd_msg, sizeof(struct comm_message));
}
