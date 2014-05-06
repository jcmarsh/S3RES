/*
 * Artificial Potential controller stand alone.
 * This variation uses file descriptors for I/O (for now just ranger and command out).
 *
 * James Marshall
 */

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "../include/taslimited.h"
#include "../include/commtypes.h"
#include "../include/statstime.h"
#include "../include/fd_client.h"

int read_in_fd;
int write_out_fd;

// TAS related
cpu_speed_t cpu_speed;

void enterLoop();
int initReplica();

void restartHandler(int signo) {
  pid_t currentPID = 0;
  // fork
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // child sets new id, recreates connects, loops
      initReplica();
      // Get own pid, send to voter
      currentPID = getpid();
      connectRecvFDS(currentPID, &read_in_fd, &write_out_fd);
      enterLoop(); // return to normal
    } else {   // Parent just returns
      return;
    }
  } else {
    printf("Fork error!\n");
    return;
  }
}

int parseArgs(int argc, const char **argv) {
  int i;
  pid_t pid;

  // TODO: error checking
  if (argc < 3) { // Must request fds
    pid = getpid();
    connectRecvFDS(pid, &read_in_fd, &write_out_fd);
  } else {
    read_in_fd = atoi(argv[1]);
    write_out_fd = atoi(argv[2]);
  }

  return 0;
}

// Should probably separate this out correctly
// Basically the init function
int initReplica() {
  int scheduler;
  struct sched_param param;

  InitTAS(DEFAULT_CPU, &cpu_speed);

  scheduler = sched_getscheduler(0);
  printf("Empty Scheduler: %d\n", scheduler);

  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }
  return 0;
}

void enterLoop() {
  timestamp_t last; //DELME
  timestamp_t current;
  void * update_id;
  int index;

  double ranger_ranges[365]; // 365 comes from somewhere in Player as the max.
  double pos[3];
  double goal[3];

  int read_ret;
  struct comm_header hdr;
  struct comm_range_data_msg recv_msg; // largest message
  struct comm_mov_cmd_msg send_msg;
 
  while(1) {
    // Blocking, but that's okay with me
    read_ret = read(read_in_fd, &recv_msg, sizeof(struct comm_range_data_msg));
    if (read_ret > 0) {
      switch (recv_msg.hdr.type) {
      case COMM_RANGE_DATA:

	hdr.type = COMM_MOV_CMD;
	hdr.byte_count = 2 * sizeof(double);
	send_msg.hdr = hdr;
	send_msg.vel_cmd[0] = 0.1;
	send_msg.vel_cmd[1] = 0.0;
	//	last = generate_timestamp(); // DELME
	write(write_out_fd, &send_msg, sizeof(struct comm_header) + hdr.byte_count);
	//	current = generate_timestamp();
	//	printf("%lld to write %d bytes\n", current - last, sizeof(struct comm_header) + hdr.byte_count); // DELME
	
	break;
      case COMM_POS_DATA:

	break;
      case COMM_WAY_RES:

	break;
      default:
	// TODO: Fail? or drop data?
	printf("ERROR: art_pot_p can't handle comm type: %d\n", hdr.type);
      }
    } else if (read_ret == -1) {
      perror("Blocking, eh?");
    } else {
      puts("ArtPot read_ret == 0?");
    }
  }
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initReplica() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  enterLoop();

  return 0;
}

