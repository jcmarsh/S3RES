/*
 * Manages a group of replicas
 *
 * March 17, 2014 James Marshall
 */

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

int initReplicas(struct replica* replicas, int num);

void replicaCrash(pid_t pid);
