/*
 * Manages a group of replicas
 *
 * March 17, 2014 James Marshall
 */

#ifndef __REP_GUARD
#define __REP_GUARD
//#include <sys/ptrace.h>
#include <fcntl.h> // needed to deal with pipes
#include <sys/user.h> // has pid_t
#include <signal.h>

#include <stdio.h>

typedef enum {
  RUNNING,
  CRASHED,
  FINISHED
} replica_status; 

 // Represents one redundant execution, implemented as a thread
struct replica {
  pid_t pid; // The pid of the thread
  int priority; // Not yet implemented
  int fd_into_rep[2]; // pipe to communicate with controller
  int fd_outof_rep[2];
  // Possibly put a pointer to entry function
  replica_status status;
};  

struct replica_group {
  struct replica* replicas;
  int num;
}; 

/*
 *
 */
int initReplicas(struct replica_group* rg, struct replica* reps, int num);

/*
 *
 */
int forkSingleReplicaNoFD(struct replica_group* rg, int num, char* prog_name);

/*
 *
 */
int forkSingleReplica(struct replica_group* rg, int num, char* prog_name);

/*
 *
 */
void replicaCrash(struct replica_group* rg, pid_t pid);

#endif // __REP_GUARD
