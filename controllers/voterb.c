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

char* controller_name;
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
int parseArgs(int argc, const char **argv);
int main(int argc, const char **argv);
void doOneUpdate();
void processOdom();
void processRanger();
void resetVotingState();
void sendWaypoints(int replica_num);
void processVelCmdFromRep(double cmd_vel_x, double cmd_vel_a, int replica_num);
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
      close(repGroup.replicas[index].fd_into_rep[0]);
      close(repGroup.replicas[index].fd_into_rep[1]);
      close(repGroup.replicas[index].fd_outof_rep[0]);
      close(repGroup.replicas[index].fd_outof_rep[1]);
      
      if (pipe(repGroup.replicas[index].fd_into_rep) == -1) {
	perror("replicas pipe error!");
      }
      
      if (pipe(repGroup.replicas[index].fd_outof_rep) == -1) {
	perror("replicas pipe error!");
      }

      repGroup.replicas[index].pid = -1;
      repGroup.replicas[index].priority = -1;
      repGroup.replicas[index].status = RUNNING;
      
      // send new pipe through fd server (should have a request)
      acceptSendFDS(&sd, &(repGroup.replicas[index].pid), repGroup.replicas[index].fd_into_rep[0], repGroup.replicas[index].fd_outof_rep[1]);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
int forkReplicas(struct replica_group* rg) {
  int index = 0;
  pid_t new_pid;

  // Fork children
  for (index = 0; index < rg->num; index++) {
    forkSingleReplicaNoFD(rg, index, controller_name);
    // TODO: Handle possible errors

    // send fds
    acceptSendFDS(&sd, &new_pid, rg->replicas[index].fd_into_rep[0], rg->replicas[index].fd_outof_rep[1]);
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
  createFDS(&sd, controller_name);

  // Let's try to launch the replicas
  initReplicas(&repGroup, replicas, REP_COUNT);
  forkReplicas(&repGroup);

  return 0;
}

int parseArgs(int argc, const char **argv) {
  int i;

  if (argc < 3) {
    puts("Usage: VoterB <controller_name> <read_in_fd> <write_out_fd>");
    return -1;
  }

  controller_name = const_cast<char*>(argv[1]);
  read_in_fd = atoi(argv[2]);
  write_out_fd = atoi(argv[3]);

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

  while(1) {
    doOneUpdate();
  }

  return 0;
}

void doOneUpdate() {
  int index = 0;
  int retval = 0;
  struct comm_range_pose_data recv_r_p_msg;
  struct comm_mov_cmd recv_m_msg;

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
    rep_pipe_r = replicas[index].fd_outof_rep[0];
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
      retval = read(read_in_fd, &recv_r_p_msg, sizeof(struct comm_range_pose_data));
      if (retval > 0) {
        // TODO: check for errors
        // Range data recieved, send to reps!
        commCopyRanger(&recv_r_p_msg, ranger_ranges, pos);
        processRanger();
      }
    }
    
    // Check all replicas for data
    for (index = 0; index < REP_COUNT; index++) {
      if (FD_ISSET(replicas[index].fd_outof_rep[0], &select_set)) {
        retval = read(replicas[index].fd_outof_rep[0], &recv_m_msg, sizeof(struct comm_mov_cmd));
        if (retval > 0) {
          //TODO: Error checking
          processVelCmdFromRep(recv_m_msg.vel_cmd[0], recv_m_msg.vel_cmd[1], index);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process ranger data
void processRanger() {
  int index = 0;

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
    commSendRanger(replicas[index].fd_into_rep[1], ranger_ranges, pos);
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
void sendWaypoints(int rep_num) {
  int index = 0;
  bool all_sent = true;
  // For now only one waypoint at a time (it's Art Pot, so fine.)
 
  // if replica already has latest... errors
  if (sent[rep_num] == true) {
    puts("SEND WAYPOINT ERROR: requester already has latest points.");
    return;
  } else { // send and mark sent
    sent[rep_num] = true;

    commSendWaypoints(replicas[rep_num].fd_into_rep[1], curr_goal[INDEX_X], curr_goal[INDEX_Y], curr_goal[INDEX_A]);
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
    struct comm_mov_cmd msg;

    msg.vel_cmd[0] = cmds[1][0];
    msg.vel_cmd[1] = cmds[1][1];

    write(write_out_fd, &msg, sizeof(struct comm_mov_cmd));
    resetVotingState();
  } else if (all_reporting) {
    // Should put the correct command
    // restart failed rep
    // reset voting state.

    // For now the timer will go off and trigger a recovery.
  }
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
