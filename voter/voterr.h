/*
 * Voter intended only for RT components (strict priority, tighter timeouts, possible protection measures).
 *
 * Author - James Marshall
 */

#include <stdbool.h>
#include <fcntl.h>
#include <linux/un.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include "controller.h"
#include "bench_config.h"
#include "system_config.h"
#include "tas_time.h"
//#include "commtypes.h"


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
int acceptSendFDS(struct server_data * sd, struct replicaR * rep, char **rep_info_in, char **rep_info_out, int pinned_cpu);

// VoterR Functions
// int -> recv -> sendCollect -> vote -> output -> recv
//                               |-> recover -^
int  initVoterD(void);
void recvData(void);
void sendCollect(int active_index);
void vote(void);
void recover(void);
void output(void);
