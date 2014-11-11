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
struct replica replicas[REP_COUNT];

// The voting information and input duplication stuff could be part of the replica struct....

// Voting stuff
voting_status vote_stat;
bool reporting[REP_COUNT];

// TAS Stuff
cpu_speed_t cpu_speed;

// FD server
struct server_data sd;

char* controller_name;
// pipes to external components (not replicas)
int pipe_count = 0;
struct typed_pipe ext_pipes[PIPE_LIMIT];

// restart timer fd
char timeout_byte[1] = {'*'};
int timeout_fd[2];
timer_t timerid;
struct itimerspec its;

// Functions!
int initVoterC();
int parseArgs(int argc, const char **argv);
int main(int argc, const char **argv);
void doOneUpdate();
void processData(struct typed_pipe pipe);
void resetVotingState();
void processFromRep(struct typed_pipe pipe, int replica_num);

void timeout_sighandler(int signum) {//, siginfo_t *si, void *data) {
  if (vote_stat == VOTING) {
    assert(write(timeout_fd[1], timeout_byte, 1) == 1);
  }
}

/*
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
*/

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

  resetVotingState();

  // Setup fd server
  createFDS(&sd, controller_name);

  // Let's try to launch the replicas
  initReplicas((struct replica **) &replicas, REP_COUNT, controller_name);
  createPipes((struct replica **) &replicas, REP_COUNT, ext_pipes, pipe_count);
  forkReplicas((struct replica **) &replicas, REP_COUNT);

  return 0;
}

int parseArgs(int argc, const char **argv) {
  if (argc < 3) {
    puts("Usage: VoterC <controller_name> <message_type:fd_in:fd_out> <...>");
    return -1;
  }

  controller_name = const_cast<char*>(argv[1]);
  for (int i = 0; i < argc - 2; i++) {
    deserializePipe(argv[i + 2], &ext_pipes[pipe_count]);
    pipe_count++;
  }

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

  struct timeval select_timeout;
  fd_set select_set;
  int max_fd;
  int rep_pipe_r;

  // See if any of the read pipes have anything
  select_timeout.tv_sec = 1;
  select_timeout.tv_usec = 0;

  FD_ZERO(&select_set);
  // Check for timeouts
  FD_SET(timeout_fd[0], &select_set);
  max_fd = timeout_fd[0];
  // Check external in pipes
  for (index = 0; index < pipe_count; index++) {
    if (ext_pipes[index].fd_in != 0) {
      int e_pipe_fd = ext_pipes[index].fd_in;
      FD_SET(e_pipe_fd, &select_set);
      if (e_pipe_fd > max_fd) {
        max_fd = e_pipe_fd;
      }
    }
  }
  // Check pipes from replicas
  for (index = 0; index < REP_COUNT; index++) {
    for (int p_index = 0; p_index < replicas[index].pipe_count; p_index++) {
      rep_pipe_r = replicas[index].vot_pipes[p_index].fd_in;
      if (rep_pipe_r != 0) {
        FD_SET(rep_pipe_r, &select_set);      
        if (rep_pipe_r > max_fd) {
          max_fd = rep_pipe_r;
        }
      }
    }
  }

  // This will wait at least timeout until return. Returns earlier if something has data.
  retval = select(max_fd + 1, &select_set, NULL, NULL, &select_timeout);

  if (retval > 0) {
    // Check for failed replica (time out)
    if (FD_ISSET(timeout_fd[0], &select_set)) {
      retval = read(timeout_fd[0], timeout_byte, 1);
      if (retval > 0) {
        printf("VoterC: CAN't CALL restarting replica\n");
        //restartReplica();
      } else {
        // TODO: Do I care about this?
      }
    }
    
    // Check for data from externel sources
    for (index = 0; index < pipe_count; index++) {
      int read_fd = ext_pipes[index].fd_in;
      if (read_fd != 0) {
        if (FD_ISSET(read_fd, &select_set)) {
          ext_pipes[index].buff_count = read(read_fd, ext_pipes[index].buffer, 1024); // TODO remove magic number
          processData(ext_pipes[index]);
        }
      }
    }

    // Check all replicas for data
    for (index = 0; index < REP_COUNT; index++) {
      for (int p_index = 0; p_index < replicas[index].pipe_count; p_index++) {
        struct typed_pipe curr_pipe = replicas[index].vot_pipes[p_index];
        if (curr_pipe.fd_in !=0) {
          if (FD_ISSET(curr_pipe.fd_in, &select_set)) {
            curr_pipe.buff_count = read(curr_pipe.fd_in, curr_pipe.buffer, 1024);
            if (curr_pipe.buffer > 0) {
              processFromRep(curr_pipe, index);  
            }
          }
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process data
// TODO: This isn't really meant to handle multiple inputs yet
void processData(struct typed_pipe pipe) {
  int index = 0;

  // TODO Voting should be per-pipe
  vote_stat = VOTING;

  // Arm timer
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = 0;
  its.it_value.tv_sec = 0;
  its.it_value.tv_nsec = PERIOD_NSEC;

  if (timer_settime(timerid, 0, &its, NULL) == -1) {
    perror("VoterC timer_settime failed");
  }

  // This has to find the matched pipes. Shitty.
  for (index = 0; index < REP_COUNT; index++) {
    // TODO: this just matches direction and type
    for (int p_index = 0; p_index < replicas[index].pipe_count; p_index++) {
      if (replicas[index].vot_pipes[p_index].fd_out != 0 &&
          replicas[index].vot_pipes[p_index].type == pipe.type) {
        int written = write(replicas[index].vot_pipes[p_index].fd_out, pipe.buffer, pipe.buff_count);
        if (written != sizeof(pipe.buff_count)) {
          perror("VoterC failed write to replica\n");
        }
      }
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

// TODO: Should be per pipe as well
char rep_outputs[REP_COUNT][1024];
int rep_output_c[REP_COUNT];
////////////////////////////////////////////////////////////////////////////////
// Process output from replica; vote on it
void processFromRep(struct typed_pipe pipe, int replica_num) {
  int index = 0;
  bool all_reporting = true;
  bool all_agree = true;

  if (reporting[replica_num] == true) {
    printf("ERROR: Replica already voted\n");
  } else {
    // record vote
    reporting[replica_num] = true;
    rep_output_c[replica_num] = pipe.buff_count;
    memcpy(rep_outputs[replica_num], pipe.buffer, pipe.buff_count);
  }
 
  for (index = 0; index < REP_COUNT - 1; index++) {
    // Check that all have reported
    all_reporting = all_reporting && reporting[index];

    // Check that all agree
    if (memcmp(rep_outputs[index], rep_outputs[index+1], rep_output_c[replica_num]) == 0) {
      // all_agree stays true
    } else {
      all_agree = false;
    }
  }
  all_reporting = all_reporting && reporting[REP_COUNT - 1];

  if (all_reporting && all_agree) {
    // Voting was successful
    // TODO, this should be matched from ext pipe to int pipes
    // Right now this just outputs to any matching pipe
    for (index = 0; index < pipe_count; index++) {
      if (ext_pipes[index].fd_out != 0 &&
          ext_pipes[index].type == pipe.type) {
        write(ext_pipes[index].fd_out, rep_outputs[0], rep_output_c[0]);
      }
    }
    resetVotingState();
  } else if (all_reporting) {
    // Should put the correct command
    // restart failed rep
    // reset voting state.

    // For now the timer will go off and trigger a recovery.
  } else {
    // Awaiting at least one more replica
  }
}
