/*
 * Second try at a Voter driver. 
 *
 * Designed to handle local navigation using three Art Pot controllers
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
#include "../include/fd_server.h"

#define SIG SIGRTMIN + 7
#define REP_COUNT 3
#define INIT_ROUNDS 4
#define PERIOD_NSEC 120000 // Max time for voting in nanoseconds (120 micro seconds)

// Either waiting for replicas to vote or waiting for the next round (next ranger input).
// Or a replica has failed and recovery is needed
typedef enum {
  VOTING,
  RECOVERY,
  WAITING
} voting_status;

// Replica related data
struct replica_group repGroup;
struct replica replicas[REP_COUNT];

// The voting information and input duplication stuff could be part of the replica struct....
// Input Duplication stuff
bool sent[REP_COUNT];
double curr_goal[3]; // Current goal for planners
double next_goal[3]; // Next goal for planners

// Voting stuff
voting_status vote_stat;
bool reporting[REP_COUNT];
double cmds[REP_COUNT][2];

// TAS Stuff
cpu_speed_t cpu_speed;

// FD server
struct server_data sd;

// FDs to the benchmarker
int read_in_fd;
int write_out_fd;

// restart timer fd
char timeout_byte[1] = {'*'};
int timeout_fd[2];
timer_t timerid;
struct itimerspec its;

// Data buffers
double pos[3];
int range_count;
double ranger_ranges[365]; // 365 is the max from a Player Ranger

// Functions!
int forkReplicas(struct replica_group* rg);
int initVoterB();
void requestWaypoints();
int parseArgs(int argc, const char **argv);
int main(int argc, const char **argv);
void doOneUpdate();
void processOdom();
void processRanger();
void resetVotingState();
void sendWaypoints(int replica_num);
void processVelCmdFromRep(double cmd_vel_x, double cmd_vel_a, int replica_num);
void putCommand(double cmd_speed, double cmd_turnrate);
void processCommand();

void timeout_sighandler(int signum) {//, siginfo_t *si, void *data) {
  if (vote_stat == VOTING) {
    assert(write(timeout_fd[1], timeout_byte, 1) == 1);
  }
}

void restartReplica() {
  int restart_id;
  int index;

  for (index = 0; index < REP_COUNT; index++) {
    if (reporting[index] == false) {
      // This is the failed replica, restart it
      // Send a signal to the rep's friend
      restart_id = (index + (REP_COUNT - 1)) % REP_COUNT; // Plus 2 is minus 1!
      // printf("Restarting %d, %d now!\n", index, restart_id);
      kill(repGroup.replicas[restart_id].pid, SIGUSR1);
      
      // clean up old data about dearly departed
      // TODO: migrate to replicas.cpp in a sane fashion
      close(repGroup.replicas[index].pipefd_into_rep[0]);
      close(repGroup.replicas[index].pipefd_into_rep[1]);
      close(repGroup.replicas[index].pipefd_outof_rep[0]);
      close(repGroup.replicas[index].pipefd_outof_rep[1]);
      
      if (pipe(repGroup.replicas[index].pipefd_into_rep) == -1) {
	perror("replicas pipe error!");
      }
      
      if (pipe(repGroup.replicas[index].pipefd_outof_rep) == -1) {
	perror("replicas pipe error!");
      }

      repGroup.replicas[index].pid = -1;
      repGroup.replicas[index].priority = -1;
      repGroup.replicas[index].status = RUNNING;
      
      // send new pipe through fd server (should have a request)
      acceptSendFDS(&sd, &(repGroup.replicas[index].pid), repGroup.replicas[index].pipefd_into_rep[0], repGroup.replicas[index].pipefd_outof_rep[1]);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
int forkReplicas(struct replica_group* rg) {
  int index = 0;
  pid_t new_pid;

  // Fork children
  for (index = 0; index < rg->num; index++) {
    forkSingleReplicaNoFD(rg, index, "ArtPot");
    // TODO: Handle possible errors

    // send fds
    acceptSendFDS(&sd, &new_pid, rg->replicas[index].pipefd_into_rep[0], rg->replicas[index].pipefd_outof_rep[1]);
    assert(new_pid == rg->replicas[index].pid);
  }

  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterB() {
  int index = 0;
  struct sigevent sev;
  struct sigaction sa;
  sigset_t mask;

  InitTAS(DEFAULT_CPU, &cpu_speed, 3);

  // timeout_fd
  if (pipe(timeout_fd) == -1) {
    perror("voterb time out pipe fail");
    return -1;
  }

  // create timer
  /* Establish handler for timer signal */
  //  sa.sa_flags = SA_SIGINFO;
  //sa.sa_sigaction = timeout_sighandler;
  //sigemptyset(&sa.sa_mask);
  //if (sigaction(SIG, &sa, NULL) == -1) {
  if (signal(SIG, timeout_sighandler) == SIG_ERR) {
    perror("VoterB sigaction failed");
    return -1;
  }

  sigemptyset(&mask);
  sigaddset(&mask, SIG);
  if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
    perror("VoterB sigprockmask failed");
    return -1;
  }

  /* Create the timer */
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIG;
  sev.sigev_value.sival_ptr = &timerid;
  if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
    perror("VoterB timer_create failed");
    return -1;
  }

  curr_goal[INDEX_X] = curr_goal[INDEX_Y] = curr_goal[INDEX_A] = 0.0;
  next_goal[INDEX_X] = next_goal[INDEX_Y] = next_goal[INDEX_A] = 0.0;
  for (index = 0; index < REP_COUNT; index++) {
    sent[index] = false;
  }

  resetVotingState();

  // Setup fd server
  createFDS(&sd);

  // Let's try to launch the replicas
  initReplicas(&repGroup, replicas, REP_COUNT);
  forkReplicas(&repGroup);

  return 0;
}

void requestWaypoints() {
  struct comm_way_req_msg send_msg;
  int retval;

  send_msg.hdr.type = COMM_WAY_REQ;
  send_msg.hdr.byte_count = 0;

  retval = write(write_out_fd, &send_msg, sizeof(struct comm_way_req_msg));
  assert(retval == sizeof(struct comm_way_req_msg));
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
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initVoterB() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  requestWaypoints();

  while(1) {
    doOneUpdate();
  }

  return 0;
}

void doOneUpdate() {
  timestamp_t current;
  int index = 0;
  int restart_id = -1;

  int retval = 0;

  struct comm_range_pose_data_msg recv_msg;
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
  FD_SET(timeout_fd[0], &select_set);
  if (timeout_fd[0] > max_fd) {
    max_fd = timeout_fd[0];
  }
  for (index = 0; index < REP_COUNT; index++) {
    rep_pipe_r = replicas[index].pipefd_outof_rep[0];
    if (rep_pipe_r > max_fd) {
      max_fd = rep_pipe_r;
    }
    FD_SET(rep_pipe_r, &select_set);
  }

  // This will wait at least timeout until return. Returns earlier if something has data.
  retval = select(max_fd + 1, &select_set, NULL, NULL, &select_timeout);

  if (retval > 0) {
    // Check for failed replica (time out)
    if (FD_ISSET(timeout_fd[0], &select_set)) {
      retval = read(timeout_fd[0], timeout_byte, 1);
      if (retval > 0) {
	printf("VoterB restarting replica\n");
	restartReplica();
      } else {
	// TODO: Do I care about this?
      }
    }
    
    // Check for data from the benchmarker
    if (FD_ISSET(read_in_fd, &select_set)) {
      retval = read(read_in_fd, &recv_msg, sizeof(struct comm_range_pose_data_msg));;
      if (retval > 0) {
	switch (recv_msg.hdr.type) {
	case COMM_RANGE_POSE_DATA:
	  // send to reps!
	  range_count = 16;
	  for (index = 0; index < range_count; index++) {
	    ranger_ranges[index] = recv_msg.ranges[index];
	  }
	  for (index = 0; index < 3; index++) {
	    pos[index] =  recv_msg.pose[index];
	  }
	  processRanger();      
	  break;
	case COMM_WAY_RES:
	  // New waypoints from benchmarker!
	  for (index = 0; index < 3; index++) {
	    next_goal[index] = ((struct comm_way_res_msg*) (&recv_msg))->point[index];
	  }
	  processCommand();
	  break;
	default:
	  printf("ERROR: VoterB can't handle comm type: %d\n", recv_msg.hdr.type);
	}
      }
    }
    
    // Check all replicas for data
    for (index = 0; index < REP_COUNT; index++) {
      // clear comm_header for next message
      recv_msg.hdr.type = -1;
      
      if (FD_ISSET(replicas[index].pipefd_outof_rep[0], &select_set)) {
	retval = read(replicas[index].pipefd_outof_rep[0], &recv_msg, sizeof(struct comm_range_pose_data_msg));
	if (retval > 0) {
	  switch (recv_msg.hdr.type) {
	  case COMM_WAY_REQ:
	    sendWaypoints(index);
	    break;
	  case COMM_MOV_CMD:
	    cmd_vel[0] = ((struct comm_mov_cmd_msg*) (&recv_msg))->vel_cmd[0];
	    cmd_vel[1] = ((struct comm_mov_cmd_msg*) (&recv_msg))->vel_cmd[1];
	    processVelCmdFromRep(cmd_vel[0], cmd_vel[1], index);
	    break;
	  default:
	    printf("ERROR: VoterB can't handle comm type: %d\n", recv_msg.hdr.type);
	  }
	}
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process ranger data
void processRanger() {
  int index = 0;
  int retval;
  struct comm_range_pose_data_msg send_msg;

  send_msg.hdr.type = COMM_RANGE_POSE_DATA;
  send_msg.hdr.byte_count = 19 * sizeof(double);

  for (index = 0; index < range_count; index++) {
    send_msg.ranges[index] = ranger_ranges[index];
  }

  for (index = 0; index < 3; index++) {
    send_msg.pose[index] = pos[index];
  }

  vote_stat = VOTING;

  // Arm timer
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = 0;
  its.it_value.tv_sec = 0;
  its.it_value.tv_nsec = PERIOD_NSEC;

  if (timer_settime(timerid, 0, &its, NULL) == -1) {
    perror("VoterB timer_settime failed");
  }

  for (index = 0; index < REP_COUNT; index++) {
    // Write range data
    retval = write(replicas[index].pipefd_into_rep[1], (void*)(&send_msg), sizeof(struct comm_range_pose_data_msg));
    assert(retval == sizeof(struct comm_range_pose_data_msg));
  }
}

////////////////////////////////////////////////////////////////////////////////
// reset / init voting state
void resetVotingState() {
  int i = 0;
  vote_stat = WAITING;

  for (i = 0; i < REP_COUNT; i++) {
    reporting[i] = false;
    cmds[i][0] = 0.0;
    cmds[i][1] = 0.0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// handle the request for inputs
// This is the primary input to the replicas, so make sure it is duplicated
void sendWaypoints(int replica_num) {
  int index = 0;
  int retval;
  bool all_sent = true;
  struct comm_header hdr;
  struct comm_way_res_msg message;
  
  // For now only one waypoint at a time (it's Art Pot, so fine.)
 
  // if replica already has latest... errors
  if (sent[replica_num] == true) {
    puts("SEND WAYPOINT ERROR: requester already has latest points.");
    return;
  } else { // send and mark sent
    sent[replica_num] = true;

    hdr.type = COMM_WAY_RES;
    hdr.byte_count = 3 * sizeof(double);
    message.hdr = hdr;
    message.point[INDEX_X] = curr_goal[INDEX_X];
    message.point[INDEX_Y] = curr_goal[INDEX_Y];
    message.point[INDEX_A] = curr_goal[INDEX_A];

    retval = write(replicas[replica_num].pipefd_into_rep[1], (void*)(&message), sizeof(struct comm_way_res_msg));
    assert(retval == sizeof(struct comm_way_res_msg));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process velocity command from replica
// This is the output from the replicas, so vote on it.
void processVelCmdFromRep(double cmd_vel_x, double cmd_vel_a, int replica_num) {
  int index = 0;
  bool all_reporting = true;
  bool all_agree = true;
  double cmd_vel = 0.0;
  double cmd_rot_vel = 0.0;

  if (reporting[replica_num] == true) {
    // If vote is same as previous, then ignore.
    if ((cmds[replica_num][0] == cmd_vel_x) &&
    	(cmds[replica_num][1] == cmd_vel_a)) {
      // Ignore
    } else {
      puts("PROBLEMS VOTING");
    }
  } else {
    // record vote
    reporting[replica_num] = true;
    cmds[replica_num][0] = cmd_vel_x;
    cmds[replica_num][1] = cmd_vel_a;
  }
 
  cmd_vel = cmds[0][0];
  cmd_rot_vel = cmds[0][1];
  for (index = 0; index < REP_COUNT; index++) {
    // Check that all have reported
    all_reporting = all_reporting && reporting[index];

    // Check that all agree
    if (cmd_vel == cmds[index][0] && cmd_rot_vel == cmds[index][1]) {
      // all_agree stays true
    } else {
      all_agree = false;
    }
  }

  if (all_reporting && all_agree) {
    putCommand(cmd_vel, cmd_rot_vel);
    resetVotingState();
  } else if (all_reporting) {
    // Should put the correct command
    // restart failed rep
    // reset voting state.

    // For now the timer will go off and trigger a recovery.
  }
}

////////////////////////////////////////////////////////////////////////////////
// Send commands to underlying position device
void putCommand(double cmd_speed, double cmd_turnrate) {
  int retval;
  struct comm_header hdr;
  struct comm_mov_cmd_msg message;

  hdr.type = COMM_MOV_CMD;
  hdr.byte_count = 2 * sizeof(double);
  message.hdr = hdr;
  message.vel_cmd[0] = cmd_speed;
  message.vel_cmd[1] = cmd_turnrate;

  retval = write(write_out_fd, (void*)&message, sizeof(struct comm_mov_cmd_msg));
  assert(retval == sizeof(struct comm_mov_cmd_msg));
}

////////////////////////////////////////////////////////////////////////////////
// Check for new commands from the server
void processCommand() {
  bool all_sent = true;
  bool non_sent = false;
  int index = 0;

  // next_goal has been updated by the main read loop
  // if all three are waiting, move to current
  for (index = 0; index < REP_COUNT; index++) {
    all_sent = all_sent && sent[index];
    non_sent = non_sent || sent[index];
  }
  if (all_sent || !non_sent) {
    curr_goal[INDEX_X] = next_goal[INDEX_X];
    curr_goal[INDEX_Y] = next_goal[INDEX_Y];
    curr_goal[INDEX_A] = next_goal[INDEX_A];
  } 
}
