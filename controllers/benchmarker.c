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

// Replica related data
struct replica_group repGroup;
struct replica replicas[REP_COUNT];

// TAS Stuff
cpu_speed_t cpu_speed;

int read_in_fd; // These are to your parent (Translator)
int write_out_fd;

timestamp_t last;

struct comm_range_data_msg range_data_msg;
struct comm_pos_data_msg pos_data_msg;
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

int initBenchMarker() {
  InitTAS(3, &cpu_speed);

  initReplicas(&repGroup, replicas, REP_COUNT);
  //forkSingleReplica(&repGroup, 0, "art_pot_p");
  forkSingleReplica(&repGroup, 0, "VoterB");

  return 0;
}

int parseArgs(int argc, const char **argv) {
  int i;

  if (argc < 3) {
    puts("Usage: VoterB <read_in_fd> <write_out_fd>");
    return -1;
  }

  read_in_fd = atoi(argv[1]);
  write_out_fd = atoi(argv[2]);

  return 0;
}

int main(int argc, const char **argv) {
  printf("Hello darkness my old friend.\n");

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

  struct comm_header hdr;
  double cmd_vel[2];

  struct timeval select_timeout;
  fd_set select_set;
  int max_fd;
  int rep_pipe_r;

  // See if any of the read pipes have anything
  select_timeout.tv_sec = 1;
  select_timeout.tv_usec = 0;

  FD_ZERO(&select_set);
  FD_SET(read_in_fd, &select_set);
  max_fd = read_in_fd;
  rep_pipe_r = replicas[0].pipefd_outof_rep[0];
  if (rep_pipe_r > max_fd) {
    max_fd = rep_pipe_r;
  }
  FD_SET(rep_pipe_r, &select_set);

  // This will wait at least timeout until return. Returns earlier if something has data.
  retval = select(max_fd + 1, &select_set, NULL, NULL, &select_timeout);

  select_timeout.tv_sec = 0;
  select_timeout.tv_usec = 0;

  // Check for data from translator...
  FD_ZERO(&select_set);
  FD_SET(read_in_fd, &select_set);
  retval = select(read_in_fd + 1, &select_set, NULL, NULL, &select_timeout);
  if (retval > 0) {
    retval = read(read_in_fd, &hdr, sizeof(struct comm_header));;
    if (retval > 0) {
      assert(retval == sizeof(struct comm_header));
      switch (hdr.type) {
      case COMM_RANGE_DATA:
	retval = read(read_in_fd, range_data_msg.ranges, hdr.byte_count);
	assert(retval == hdr.byte_count);
	processRanger();      
	break;
      case COMM_POS_DATA:
	retval = read(read_in_fd, pos_data_msg.pose, hdr.byte_count);
	assert(retval == hdr.byte_count);
	processOdom();
	break;
      case COMM_WAY_RES:
	retval = read(read_in_fd, way_res_msg.point, hdr.byte_count);
	assert(retval == hdr.byte_count);
	sendWaypoints();
	break;
      default:
	printf("ERROR: VoterB can't handle comm type: %d\n", hdr.type);
      }
    }
  }

  // clear comm_header for next message
  hdr.type = -1;
  hdr.byte_count = -1;

  select_timeout.tv_sec = 0;
  select_timeout.tv_usec = 0;

  // Check for data from translator...
  FD_ZERO(&select_set);
  FD_SET(replicas[0].pipefd_outof_rep[0], &select_set);
  retval = select(replicas[0].pipefd_outof_rep[0] + 1, &select_set, NULL, NULL, &select_timeout);
  if (retval > 0) {
    // Check replica for data
    retval = read(replicas[0].pipefd_outof_rep[0], &hdr, sizeof(struct comm_header));
    if (retval > 0) {
      switch (hdr.type) {
      case COMM_WAY_REQ:
	requestWaypoints();
	break;
      case COMM_MOV_CMD:
	retval = read(replicas[0].pipefd_outof_rep[0], mov_cmd_msg.vel_cmd, hdr.byte_count);
	assert(retval == hdr.byte_count);
	processCommand();
	break;
      default:
	printf("ERROR: VoterB can't handle comm type: %d\n", hdr.type);
      }
    }
  }
}

void sendWaypoints() {
  struct comm_header hdr;

  hdr.type = COMM_WAY_RES;
  hdr.byte_count = 3 * sizeof(double);
  way_res_msg.hdr = hdr;
  // data set by read

  write(replicas[0].pipefd_into_rep[1], (void*)(&way_res_msg), sizeof(struct comm_header) + hdr.byte_count);
}


////////////////////////////////////////////////////////////////////////////////
// Process new odometry data
void processOdom() {
  struct comm_header hdr;

  hdr.type = COMM_POS_DATA;
  hdr.byte_count = 3 * sizeof(double);
  pos_data_msg.hdr = hdr;
  // data set by read

  write(replicas[0].pipefd_into_rep[1], (void*)(&pos_data_msg), sizeof(struct comm_header) + hdr.byte_count);
}

////////////////////////////////////////////////////////////////////////////////
// Process ranger data
void processRanger() {
  int index = 0;
  struct comm_header hdr;

  hdr.type = COMM_RANGE_DATA;
  hdr.byte_count = 16 * sizeof(double);
  range_data_msg.hdr = hdr;
  // msg data set by read in

#ifdef _STATS_BENCH_ROUND_TRIP_
  last = generate_timestamp();
#endif // _STATS_BENCH_ROUND_TRIP_
#ifdef _STATS_BENCH_TO_CONT_
  printf("Benc\t%lf\n", timestamp_to_realtime(generate_timestamp(), cpu_speed));
#endif

  write(replicas[0].pipefd_into_rep[1], (void*)(&range_data_msg), sizeof(struct comm_header) + hdr.byte_count);
}

// To translator
void requestWaypoints() {
  struct comm_header hdr;
  
  hdr.type = COMM_WAY_REQ;
  hdr.byte_count = 0;

  write(write_out_fd, &hdr, sizeof(struct comm_header));
}

////////////////////////////////////////////////////////////////////////////////
// Send commands to underlying position device
void processCommand() {
  struct comm_header hdr;
#ifdef _STATS_BENCH_ROUND_TRIP_
  timestamp_t current;
  current = generate_timestamp();

  printf("%lld\n", current - last);
#endif
#ifdef _STATS_CONT_TO_BENCH_
  printf("Benc\t%lf\n", timestamp_to_realtime(generate_timestamp(), cpu_speed));
#endif

  hdr.type = COMM_MOV_CMD;
  hdr.byte_count = 2 * sizeof(double);
  mov_cmd_msg.hdr = hdr;
  // data was set by read

  write(write_out_fd, (void*)(&mov_cmd_msg), sizeof(struct comm_header) + hdr.byte_count);
}
