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

#include "../include/taslimited.h"
#include "../include/statstime.h"
#include "../include/replicas.h"
#include "../include/commtypes.h"

#define REP_COUNT 1

#define TARGET_ROW 17
#define TARGET_COL 3


// Replica related data
struct replica_group repGroup;
struct replica replicas[REP_COUNT];

// TAS Stuff
cpu_speed_t cpu_speed;

int read_in_fd; // These are to your parent (Translator)
int write_out_fd;

timestamp_t last;
long timer_interrupts;
FILE* proc_interrupts;
char i_buffer[256];

struct comm_range_pose_data_msg range_pose_data_msg;
struct comm_way_req_msg way_req_msg;
struct comm_way_res_msg way_res_msg;
struct comm_mov_cmd_msg mov_cmd_msg;

// FUNCTIONS!
int initBenchMarker();
int parseArgs(int argc, const char **argv);
void doOneUpdate();
void sendWaypoints();
void processOdom();
void processRanger();
void requestWaypoints();
void processCommand();
long parseInterrupts();

int initParser() {
  proc_interrupts = fopen("/proc/interrupts", "r");
}

long parseInterrupts() {
  int cpus_check[] = {2, 3};
  int inte_check[] = {14,15,17,19,20,21,25};
  int iindex = 0;
  const char token[2] = " ";
  int row_num = 0;
  int col_num = 0;
  char *cell_value;
  long total = 0;

  rewind(proc_interrupts);

  while(fgets(i_buffer, 256, proc_interrupts) > 0) {
    if (row_num == inte_check[iindex] and row_num < 25) {
      cell_value = strtok(i_buffer, token);
      while (cell_value != NULL) {
	cell_value = strtok(NULL, token);
	if (col_num == cpus_check[0] || col_num == cpus_check[1]) {
	  total += atol(cell_value);
	}
	col_num++;
      }
      col_num = 0;
      iindex++;
    }
    row_num++;
  }
  return total;
}


int initBenchMarker() {
  int scheduler;
  int p_offset = 0;

  InitTAS(3, &cpu_speed, 0);

  scheduler = sched_getscheduler(0);
  printf("BenchMarker Scheduler: %d\n", scheduler);

  initParser();

  initReplicas(&repGroup, replicas, REP_COUNT);
  forkSingleReplica(&repGroup, 0, "Empty");
  //forkSingleReplica(&repGroup, 0, "ArtPot");
  //forkSingleReplica(&repGroup, 0, "VoterB");

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
  struct comm_range_pose_data_msg recv_msg;

  int rep_pipe_r;

  // Message comes in from translator
  // Message goes out from replica
  // That is it. Except for those waypoint request / responses

  // Translator driven, check first. This call blocks.
  retval = read(read_in_fd, &recv_msg, sizeof(struct comm_range_pose_data_msg));
  if (retval > 0) {
    switch (recv_msg.hdr.type) {
    case COMM_RANGE_POSE_DATA:
      for (index = 0; index < 16; index++) {
	range_pose_data_msg.ranges[index] = recv_msg.ranges[index];
      }
      for (index = 0; index < 3; index++) {
	range_pose_data_msg.pose[index] = recv_msg.ranges[index];
      }
      //	  puts("Bench: Range / Pose Data");
      processRanger();      
      break;
    case COMM_WAY_RES:
      // TODO: Figure this out
      puts("DANGER WILL ROBINSON!");
      for (index = 0; index < 3; index++) {
	way_res_msg.point[index] = ((struct comm_way_res_msg*) (&recv_msg))->point[index];
      }
      sendWaypoints();
      break;
    default:
      printf("ERROR: BenchMarker can't handle comm type: %d\n", recv_msg.hdr.type);
    }
  } else {
    perror("Bench: read should have worked, failed."); // EINTR?
  }

  // Second part of the cycle: response from replica
  retval = read(replicas[0].pipefd_outof_rep[0], &recv_msg, sizeof(struct comm_range_pose_data_msg));
  if (retval > 0) {
    switch (recv_msg.hdr.type) {
    case COMM_WAY_REQ:
      puts("THIS IS NOT ACCEPTABLE");
      requestWaypoints();
      break;
    case COMM_MOV_CMD:
      mov_cmd_msg.vel_cmd[0] = ((struct comm_mov_cmd_msg*) (&recv_msg))->vel_cmd[0];
      mov_cmd_msg.vel_cmd[1] = ((struct comm_mov_cmd_msg*) (&recv_msg))->vel_cmd[1];
      //	  puts("\tBench: Command Recv.");
      processCommand();
      break;
    default:
      printf("ERROR: BenchMarker can't handle comm type: %d\n", recv_msg.hdr.type);
    }
  } else {
    perror("Bench: read should have worked, failed."); // EINTR?
  }
}

void sendWaypoints() {
  struct comm_header hdr;

  hdr.type = COMM_WAY_RES;
  hdr.byte_count = 3 * sizeof(double);
  way_res_msg.hdr = hdr;
  // data set by read

  write(replicas[0].pipefd_into_rep[1], (void*)(&way_res_msg), sizeof(struct comm_way_res_msg));
}

////////////////////////////////////////////////////////////////////////////////
// Process ranger data
void processRanger() {
  int index = 0;

  range_pose_data_msg.hdr.type = COMM_RANGE_POSE_DATA;
  range_pose_data_msg.hdr.byte_count = 19 * sizeof(double);
  // msg data set by read in

#ifdef _STATS_BENCH_ROUND_TRIP_
  last = generate_timestamp();
  timer_interrupts = parseInterrupts();
#endif // _STATS_BENCH_ROUND_TRIP_

  write(replicas[0].pipefd_into_rep[1], (void*)(&range_pose_data_msg), sizeof(struct comm_range_pose_data_msg));
}

// To translator
void requestWaypoints() {
  struct comm_header hdr;
  
  hdr.type = COMM_WAY_REQ;
  hdr.byte_count = 0;
  way_req_msg.hdr = hdr;

  write(write_out_fd, (void*)(&way_req_msg), sizeof(struct comm_way_req_msg));
}

////////////////////////////////////////////////////////////////////////////////
// Send commands to underlying position device
void processCommand() {
#ifdef _STATS_BENCH_ROUND_TRIP_
  timestamp_t current;
  long new_interrupts;

  new_interrupts = parseInterrupts();
  current = generate_timestamp();
  // check against previous interrupt count

  //  printf("%ld\t%lld\n", 42, current - last);
  printf("%ld\t%lld\n", new_interrupts - timer_interrupts, current - last);
#endif

  mov_cmd_msg.hdr.type = COMM_MOV_CMD;
  mov_cmd_msg.hdr.byte_count = 2 * sizeof(double);
  // data was set by read

  write(write_out_fd, (void*)(&mov_cmd_msg), sizeof(struct comm_mov_cmd_msg));
}
