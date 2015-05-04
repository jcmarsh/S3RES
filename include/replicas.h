/*
 * Manages a group of replicas
 *
 * March 17, 2014 James Marshall
 */

#ifndef __REP_GUARD
#define __REP_GUARD

#include "commtypes.h"
#include <fcntl.h> // needed to deal with pipes
#include <sys/user.h> // has pid_t
#include <signal.h>
#include <stdio.h>

 // Represents one redundant execution, implemented as a thread
struct replica {
  pid_t pid; // The pid of the thread
  int priority;
  char* name;

  // list of connections
  // Uses the same format as the plumber
  int pipe_count;
  struct typed_pipe vot_pipes[PIPE_LIMIT]; // Voter side of pipes
  int voter_rep_in_copy[PIPE_LIMIT];       // Voter needs a copy of the read side of rep pipes
  struct typed_pipe rep_pipes[PIPE_LIMIT]; // rep side of pipes
  int voted[PIPE_LIMIT];
};  

/*
 *
 */
void initReplicas(struct replica reps[], int num, const char* name, int priority);

/*
 *
 */
void createPipes(struct replica reps[], int rep_num, struct typed_pipe ext_pipes[], int pipe_count);
void createPipesSpecial(struct replica reps[], int rep_num, struct typed_pipe ext_pipes[], int pipe_count);
/*
 *
 */
void forkReplicas(struct replica replicas[], int rep_num);
void forkReplicasSpecial(struct replica replicas[], int rep_num);

#endif // __REP_GUARD
