/*
 * Voter intended only for RT components (strict priority, tighter timeouts, possible protection measures).
 *
 * Author - James Marshall
 */

#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>


#include "bench_config.h"
#include "system_config.h"
#include "tas_time.h"

#define PERIOD_USEC 120 // Max time for voting in micro seconds
#define VOTER_PRIO_OFFSET 5 // Replicas run with a -5 offset
#define MAX_PIPE_BUFF 4096

// fd_server structs
struct server_data {
  struct sockaddr_un address;
  socklen_t address_length;
  int sock_fd;
};

// Represents one redundant execution, implemented as a process
struct replicaR {
  int pinned_cpu;
  int priority;
  pid_t pid;
  // list of connections
  // Uses the same format as the plumber
  // The following all have in_pipe_count members
  int in_pipe_count;
  unsigned int *fd_ins;
  // The following all have out_pipe_count members
  int out_pipe_count;
  unsigned int *fd_outs;
  unsigned int *buff_counts;
  unsigned char **buffers;
};

// fd_server functions
int createFDS(struct server_data * sd, const char* name);
int acceptSendFDS(struct server_data * sd, struct replicaR * rep, char **rep_info_in, char **rep_info_out);

// VoterR Functions
int  initVoterD(void);

bool recvData(void);   // sets active_pipe_index (pipe data came in on)
bool sendToReps(void); // sends data to replica in pipe[active_pipe_index]
int  collectFromReps(bool set_timer, int rep_count); // returns number of reps that are done. Set timer and wait for rep_count responses
bool checkSDC(void);   // checkSDC will have to kill faulty replicas on its own and set fault_index
// void restartReplica(void); // restarts fault_index
void startReplicas(bool forking, int rep_index, int rep_count); // existing function, also used in initial startup
void sendData(int rep_to_send); // sends data from (TODO: Which rep?) out of voter. Reset all state for next loop
void findFaultReplica(void);    // set fault_index
