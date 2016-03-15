/*
 * Manages a group of replicas
 *
 * March 17, 2014 James Marshall
 */

#ifndef __REP_GUARD
#define __REP_GUARD

//#include "commtypes.h"

#include "../include/fd_server.h"
#include "../include/vote_buff.h"
#include "../include/system_config.h"
#include <stdio.h>

 // Represents one redundant execution, implemented as a process
struct replica {
  pid_t pid; // The pid of the process
  int priority;
  int pinned_cpu;
  char* name;

  // list of connections
  // Uses the same format as the plumber
  int pipe_count;
  struct vote_pipe *vot_pipes; // Voter side of pipes
  int *voter_rep_in_copy;       // Voter needs a copy of the read side of rep pipes
  struct vote_pipe *rep_pipes; // rep side of pipes
  // int voted[PIPE_LIMIT];
};  

void initReplicas(struct replica reps[], int rep_num, const char* name, int priority);
void cleanupReplica(struct replica reps[], int rep_index);
int bytesReady(struct replica reps[], int num, int pipe_num);
void startReplicas(struct replica reps[], int num, struct server_data *sd, const char* name, struct vote_pipe ext_pipes[], int pipe_count, int default_priority);
int behindRep(struct replica reps[], int num, int pipe_num);
void restartReplica(struct replica reps[], int num, struct server_data *sd, struct vote_pipe ext_pipes[], int restarter, int restartee, int default_priority);
void createPipes(struct replica reps[], int num, struct vote_pipe ext_pipes[], int pipe_count);

void forkReplicas(struct replica reps[], int num, int additional_argc, char **additional_argv);

#endif // __REP_GUARD
