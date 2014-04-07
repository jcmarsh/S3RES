
#include <sys/ptrace.h>


typedef enum {
  RUNNING,
  CRASHED,
  FINISHED
} replica_status; 

// replicas with no fds
struct replica_l {
  pid_t pid;
  int priority;
  replica_status status;
};

struct replica_group_l {
  struct replica_l* replicas;
  int num;
};


// Data
  // Replica related methods
  int InitReplicas(struct replica_group_l* rg, int num);
  int ForkReplicas(struct replica_group_l* rg);

  // Replica related data
  struct replica_group_l repGroup;
  struct replica_l replicas[3];

// In setup
  puts("VoterA: Init replicas");
  //  this->InitReplicas(&repGroup, 3);
  // Let's try to launch the replicas
  puts("VoterA: Fork replicas");
  //  this->ForkReplicas(&repGroup);

////////////////////////////////////////////////////////////////////////////////
int VoterADriver::InitReplicas(struct replica_group_l* rg, int num) {
  int index = 0;

  for (index = 0; index < rg->num; index++) {
    rg->replicas[index].pid = -1;
    rg->replicas[index].priority = -1;
    rg->replicas[index].status = RUNNING;
  }
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
int VoterADriver::ForkReplicas(struct replica_group_l* rg) {
  pid_t currentPID = 0;
  int index = 0;
  char rep_num[2];
  char* rep_argv[] = {"127.0.0.1", "6666", rep_num, NULL};
  char* rep_envp[] = {"PATH=/home/jcmarsh/research/PINT/controllers", NULL};

  // Fork children
  for (index = 0; index < rg->num; index++) {
    sprintf(rep_num, "%d", index+1);
    rep_argv[2] = rep_num;
    currentPID = fork();

    if (currentPID >= 0) { // Successful fork
      if (currentPID == 0) { // Child process
	puts("VoterA:ForkReplicas: Child execing");
	ptrace(PTRACE_TRACEME, 0, NULL, NULL);
	execve("art_pot_launch", &rep_argv[index], rep_envp);
      } else { // Parent Process
	rg->replicas[index].pid = currentPID;
      }
    } else {
      printf("Fork error!\n");
      return -1;
    }
  }

  return 1;
}
