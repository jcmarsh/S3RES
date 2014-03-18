/*
 * Manages a group of replicas
 *
 * March 17, 2014 James Marshall
 */

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
  int pipefd[2]; // pipe to communicate with controller
  // Possibly put a pointer to entry function
  replica_status status;
  //  unsigned long last_result; // TODO: Usefull comment.
};  

struct replica_group {
  struct replica* replicas;
  int num;
  int nfds; // Highest value fd in the read pipes
  fd_set read_fds;
}; 

/*
 *
 */
int initReplicas(struct replica_group* rg, struct replica* reps, int num);

/*
 *
 */
void replicaCrash(struct replica_group* rg, pid_t pid);

/* 
 * return 0 if the parent, - if error, the write_out fd if child
 */
int launchChildren(struct replica_group* rg);
