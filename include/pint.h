/*
 * Manages a group of replicas
 *
 * March 17, 2014 James Marshall
 */

//#include <sys/ptrace.h>
#include <wait.h>
#include <fcntl.h> // needed to deal with pipes
#include <signal.h>

#include <stdio.h>

#include <sys/user.h>
#include <sys/ptrace.h>

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
  unsigned long last_result;
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

/***********************************************/

/*
 * Operations dealing with system registers.
 * Should be able to handle x86 and x86_64 for now.
 *
 * March 17th, 2014 James Marshall
 */

// Modify the register structure to have one (uniformily distributed) bit flip.
/*
 * returns 1 if an error is 
 */
void injectRegError(pid_t pid);

/**********************************************/

int setupSignal(int signal_ignored);

int handleProcess(struct replica_group* rg, pid_t pid, int status, int insert_error);

void printResults(struct replica* replicas, int num);
