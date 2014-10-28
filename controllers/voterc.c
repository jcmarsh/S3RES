/*
 * Voter that is able to start and connect to other voters, and maybe
 * even be generic
 *
 * Author - James Marshall
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

// Voting stuff
voting_status vote_stat;
bool reporting[REP_COUNT];

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

// Functions!
int forkReplicas(struct replica_group* rg);
int initVoterC();
int parseArgs(int argc, const char **argv);
int main(int argc, const char **argv);
void doOneUpdate();
void processData(struct comm_message * msg);
void resetVotingState();
void processFromRep(struct comm_message * msg, int replica_num);

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
    // TODO: not always going to be ArtPot
    forkSingleReplicaNoFD(rg, index, "ArtPot");
    // TODO: Handle possible errors

    // send fds
    acceptSendFDS(&sd, &new_pid, rg->replicas[index].fd_into_rep[0], rg->replicas[index].fd_outof_rep[1]);
    assert(new_pid == rg->replicas[index].pid);
  }

  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterC() {
  int index = 0;
  struct sigevent sev;
  struct sigaction sa;
  sigset_t mask;

  InitTAS(DEFAULT_CPU, &cpu_speed, 3);

  // timeout_fd
  if (pipe(timeout_fd) == -1) {
    perror("VoterC time out pipe fail");
    return -1;
  }

  // create timer
  /* Establish handler for timer signal */
  //  sa.sa_flags = SA_SIGINFO;
  //sa.sa_sigaction = timeout_sighandler;
  //sigemptyset(&sa.sa_mask);
  //if (sigaction(SIG, &sa, NULL) == -1) {
  if (signal(SIG, timeout_sighandler) == SIG_ERR) {
    perror("VoterC sigaction failed");
    return -1;
  }

  sigemptyset(&mask);
  sigaddset(&mask, SIG);
  if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
    perror("VoterC sigprockmask failed");
    return -1;
  }

  /* Create the timer */
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIG;
  sev.sigev_value.sival_ptr = &timerid;
  if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
    perror("VoterC timer_create failed");
    return -1;
  }

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

int parseArgs(int argc, const char **argv) {
  int i;

  if (argc < 3) {
    puts("Usage: VoterC <read_in_fd> <write_out_fd>");
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

  if (initVoterC() < 0) {
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
  struct comm_message recv_msg;

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
        printf("VoterC restarting replica\n");
        restartReplica();
      } else {
        // TODO: Do I care about this?
      }
    }
    
    // Check for data from the benchmarker
    if (FD_ISSET(read_in_fd, &select_set)) {
      retval = read(read_in_fd, &recv_msg, sizeof(struct comm_message));
      if (retval > 0) {
        switch (recv_msg.type) {
        case COMM_WAY_RES: // can't handle yet
          // New waypoints from benchmarker!
          printf("ERROR: VoterC can't handle Waypoint responses.\n");
          break;
        default:
          processData(&recv_msg);
        }
      }
    }
    
    // Check all replicas for data
    for (index = 0; index < REP_COUNT; index++) {
      // clear comm_header for next message
      recv_msg.type = -1;
      
      if (FD_ISSET(replicas[index].fd_outof_rep[0], &select_set)) {
        retval = read(replicas[index].fd_outof_rep[0], &recv_msg, sizeof(struct comm_message));
        if (retval > 0) {
          switch (recv_msg.type) {
          case COMM_WAY_REQ:
            printf("ERROR: VoterC can't handle sending Waypoint requests.\n");
            break;
          default:
            processFromRep(&recv_msg, index);
          }
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process data
void processData(struct comm_message * msg) {
  int index = 0;

  vote_stat = VOTING;

  // Arm timer
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = 0;
  its.it_value.tv_sec = 0;
  its.it_value.tv_nsec = PERIOD_NSEC;

  if (timer_settime(timerid, 0, &its, NULL) == -1) {
    perror("VoterC timer_settime failed");
  }

  for (index = 0; index < REP_COUNT; index++) {
    int written = write(replicas[index].fd_into_rep[1], msg, sizeof(struct comm_message));
    if (written != sizeof(struct comm_message)) {
      perror("VoterC failed write to replica\n");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// reset / init voting state
void resetVotingState() {
  int i = 0;
  vote_stat = WAITING;

  for (i = 0; i < REP_COUNT; i++) {
    reporting[i] = false;
  }
}

struct comm_message rep_outputs[3];
////////////////////////////////////////////////////////////////////////////////
// Process output from replica; vote on it
void processFromRep(struct comm_message * msg, int replica_num) {
  int index = 0;
  bool all_reporting = true;
  bool all_agree = true;

  if (reporting[replica_num] == true) {
    printf("ERROR: Replica already voted\n");
  } else {
    // record vote
    reporting[replica_num] = true;
    memcpy(&rep_outputs[replica_num], msg, sizeof(comm_message));
  }
 
  for (index = 0; index < REP_COUNT - 1; index++) {
    // Check that all have reported
    all_reporting = all_reporting && reporting[index];

    // Check that all agree
    if (memcmp(&rep_outputs[index], &rep_outputs[index+1], sizeof(comm_message)) == 0) {
      // all_agree stays true
    } else {
      all_agree = false;
    }
  }
  all_reporting = all_reporting && reporting[REP_COUNT - 1];

  if (all_reporting && all_agree) {
    write(write_out_fd, &rep_outputs[0], sizeof(struct comm_message));
    resetVotingState();
  } else if (all_reporting) {
    // Should put the correct command
    // restart failed rep
    // reset voting state.

    // For now the timer will go off and trigger a recovery.
  }
}
