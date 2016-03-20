/*
 * Voter for just restarting SMR Load component.
 *
 * Author - James Marshall
 */

#include "controller.h"
#include "tas_time.h"

#include <malloc.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include "replicas.h"
 
#define PERIOD_USEC 120 // Max time for voting in micro seconds
#define VOTER_PRIO_OFFSET 5 // Replicas run with a -5 offset

pid_t last_dead = -1;

// Replica related data
struct replica replicas[1];

// TAS Stuff
int voter_priority;
int replica_priority;

// FD server
struct server_data sd;

replication_t rep_type;
int rep_count;
char* controller_name;
// pipes to external components (not replicas)
int pipe_count = 0;
struct vote_pipe *ext_pipes;

// Functions!
int initVoterS(void);
int parseArgs(int argc, const char **argv);
void doOneUpdate(void);
void processData(struct vote_pipe *pipe, int pipe_index);
void sendPipe(int pipe_num, int replica_num);
void processFromRep(int replica_num, int pipe_num);
void writeBuffer(int fd_out, unsigned char* buffer, int buff_count);

void voterRestartHandler(void) {
  int p_index;
  // Caught Exec / Control loop error

  switch (rep_type) {
    case SMR: {
      // Need to cold restart the replica
      last_dead = replicas[0].pid;
      cleanupReplica(replicas, 0);

      startReplicas(replicas, rep_count, &sd, controller_name, ext_pipes, pipe_count, replica_priority);
      
      // Resend last data
      for (p_index = 0; p_index < pipe_count; p_index++) {
        int read_fd = ext_pipes[p_index].fd_in;
        if (read_fd != 0) {
          processData(&(ext_pipes[p_index]), p_index);    
        }
      }

      break;
    }

    return;
  }
}

void doOneUpdate(void) {
  int p_index, r_index;
  int retval = 0;

  struct timeval select_timeout;
  fd_set select_set;

  // can only waitpid for children (unless subreaper is used (prctl is not POSIX compliant)).
  int exit_pid = waitpid(-1, NULL, WNOHANG); // Seems to take a while for to clean up zombies
      
  if (exit_pid > 0) {
    debug_print("%s PID %d exited on its own.\n", controller_name, exit_pid);
    voterRestartHandler();
  }

  select_timeout.tv_sec = 0;
  select_timeout.tv_usec = 50000;

  // See if any of the read pipes have anything
  FD_ZERO(&select_set);

  // Check external in pipes
  for (p_index = 0; p_index < pipe_count; p_index++) {
    if (ext_pipes[p_index].fd_in != 0) {
      int e_pipe_fd = ext_pipes[p_index].fd_in;
      FD_SET(e_pipe_fd, &select_set);
    }
  }
  
  // Check pipes from replicas
  for (p_index = 0; p_index < pipe_count; p_index++) {
    for (r_index = 0; r_index < rep_count; r_index++) {
      int rep_pipe_fd = replicas[r_index].vot_pipes[p_index].fd_in;
      if (rep_pipe_fd != 0) {
        FD_SET(rep_pipe_fd, &select_set);      
      }
    }
  }

  // This will wait at least timeout until return. Returns earlier if something has data.
  retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);

  if (retval > 0) {    
    // Check for data from external sources
    for (p_index = 0; p_index < pipe_count; p_index++) {
      int read_fd = ext_pipes[p_index].fd_in;
      if (read_fd != 0) {
        if (FD_ISSET(read_fd, &select_set)) {
          ext_pipes[p_index].buff_count = read(read_fd, ext_pipes[p_index].buffer, MAX_VOTE_PIPE_BUFF);
          if (ext_pipes[p_index].buff_count > 0) { // TODO: read may still have been interrupted
            processData(&(ext_pipes[p_index]), p_index);
          } else if (ext_pipes[p_index].buff_count < 0) {
            debug_print("Voter - read error on external pipe - Controller %s pipe %d\n", controller_name, p_index);
          } else {
            debug_print("Voter - read == 0 on external pipe - Controller %s pipe %d\n", controller_name, p_index);
          }
        }
      }
    }

    // Check all replicas for data
    for (p_index = 0; p_index < pipe_count; p_index++) {
      for (r_index = 0; r_index < rep_count; r_index++) {
        struct vote_pipe* curr_pipe = &(replicas[r_index].vot_pipes[p_index]);
        if (curr_pipe->fd_in !=0) {
          if (FD_ISSET(curr_pipe->fd_in, &select_set)) {
            processFromRep(r_index, p_index);
          }
        }
      }
    }
  }
}

void writeBuffer(int fd_out, unsigned char* buffer, int buff_count) {
  int retval = write(fd_out, buffer, buff_count);
  if (retval == buff_count) {
    // success, do nothing
  } else if (retval > 0) { // TODO: resume write? 
    debug_print("Voter wrote partial message for %s, pipe %d, bytes written: %d\texpected: %d\n", controller_name, fd_out, retval, buff_count);
  } else if (retval < 0) {
    debug_print("Voter for %s failed write fd: %d\n", controller_name, fd_out);
  } else {
    debug_print("Voter wrote == 0 for %s fd: %d\n", controller_name, fd_out);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process data
void processData(struct vote_pipe *pipe, int pipe_index) {
  int r_index;

  for (r_index = 0; r_index < rep_count; r_index++) {
    writeBuffer(replicas[r_index].vot_pipes[pipe_index].fd_out, pipe->buffer, pipe->buff_count);
  }
}

void sendPipe(int pipe_num, int replica_num) {
  int r_index;
  int bytes_avail = bytesReady(replicas, rep_count, pipe_num);
  if (bytes_avail == 0) {
    return;
  }

  for (r_index = 0; r_index < rep_count; r_index++) {
    if (replica_num == r_index) {
      int retval; // TODO: error?
      retval = buffToPipe(&(replicas[r_index].vot_pipes[pipe_num]), ext_pipes[pipe_num].fd_out, bytes_avail);
    } else {
      fakeToPipe(&(replicas[r_index].vot_pipes[pipe_num]), bytes_avail);
    }
  }
}

void checkSDC(int pipe_num) {
  int r_index;
  int bytes_avail = bytesReady(replicas, rep_count, pipe_num);
  if (bytes_avail == 0) {
    return;
  }

  switch (rep_type) {
    case SMR: 
      // Only one rep, so pretty much have to trust it
      sendPipe(pipe_num, 0);
      return;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Process output from replica; vote on it
void processFromRep(int replica_num, int pipe_num) {
  // read from pipe
  if (pipeToBuff(&(replicas[replica_num].vot_pipes[pipe_num])) == 0) {
    balanceReps(replicas, rep_count, replica_priority);
    checkSDC(pipe_num);
  } else {
    debug_print("Voter - read problem on internal pipe - Controller %s, rep %d, pipe %d\n", controller_name, replica_num, pipe_num);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterS(void) {
  replica_priority = voter_priority - VOTER_PRIO_OFFSET;

  // Setup fd server
  if (createFDS(&sd, controller_name) < 0) {
    printf("Failed to create FD server\n");
  }
  startReplicas(replicas, rep_count, &sd, controller_name, ext_pipes, pipe_count, replica_priority);

  InitTAS(VOTER_PIN, voter_priority); // IMPORTANT: Should be after forking replicas to subvert CoW

  // TODO: I did this to force page faults... but I think there was a different issue.
  // This section may no longer be necessary. Need to check with the page fault tests.
  int p_index, r_index;

  for (p_index = 0; p_index < pipe_count; p_index++) {
    int read_fd = ext_pipes[p_index].fd_in;
    if (read_fd != 0) {  // Causes page faults
      ext_pipes[p_index].buff_count = read(read_fd, ext_pipes[p_index].buffer, 0);
    }
  }

  // Check all replicas for data
  for (r_index = 0; r_index < rep_count; r_index++) {
    for (p_index = 0; p_index < replicas[r_index].pipe_count; p_index++) {
      struct vote_pipe* curr_pipe = &(replicas[r_index].vot_pipes[p_index]);
      if (curr_pipe->fd_in !=0) {
        curr_pipe->buff_count = read(curr_pipe->fd_in, curr_pipe->buffer, 0);
      }
    }
  }

  debug_print("Initializing VoterS(%s)\n", controller_name);

  return 0;
}

void parsePipe(const char* serial, struct vote_pipe* pipe) {
  char *rep_info;
  int in, out, timed, i, str_length;

  for (i = 0; i < 100; i++) {
    if (serial[i] == ':') {
      sscanf(&(serial[i]), ":%d:%d:%d", &in, &out, &timed);
      str_length = i;
      break;
    }
  }
  rep_info = (char*) malloc(sizeof(char) * str_length);
  for (i = 0; i < str_length; i++) {
    rep_info[i] = serial[i];
  }

  pipe->rep_info = rep_info;
  pipe->fd_in = in;
  pipe->fd_out = out;
  pipe->timed = timed;
}

int parseArgs(int argc, const char **argv) {
  int i;
  int required_args = 5; // voter name, controller name, rep_type, timeout and priority
  controller_name = (char*) (argv[1]);
  rep_type = reptypeToEnum((char*)(argv[2]));
  rep_count = reptypeToCount(rep_type);
  // voting_timeout = atoi(argv[3]);
  voter_priority = atoi(argv[4]);

  if (argc < required_args) { 
    puts("Usage: VoterS <controller_name> <rep_type> <timeout> <priority> <fd_in:fd_out:time> <...>");
    return -1;
  } else {
    for (i = 0; (i < argc - required_args && i < PIPE_LIMIT); i++) {
      pipe_count++;
    }
    if (pipe_count >= PIPE_LIMIT) {
      debug_print("VoterS: Raise pipe limit.\n");
    }

    ext_pipes = (struct vote_pipe *) malloc(sizeof(struct vote_pipe) * pipe_count);
    for (i = 0; (i < argc - required_args && i < PIPE_LIMIT); i++) {
      parsePipe(argv[i + required_args], &ext_pipes[i]); // TODO: WRONG! Maybe. Should ignore non-numbers to deserialize?
    }

    replicas[0].vot_pipes = (struct vote_pipe *) malloc(sizeof(struct vote_pipe) * pipe_count);
    replicas[0].voter_rep_in_copy = (int *) malloc(sizeof(int) * pipe_count);
    replicas[0].rep_pipes = (struct vote_pipe *) malloc(sizeof(struct vote_pipe) * pipe_count);
  }

  return 0;
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initVoterS() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  while(1) {
    doOneUpdate();
  }

  return 0;
}