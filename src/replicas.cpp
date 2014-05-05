#include "../include/replicas.h"

int initReplicas(struct replica_group* rg, struct replica* reps, int num) {
  int index = 0;

  rg->replicas = reps;
  rg->num = num;
  
  // Init three replicas
  for (index = 0; index < rg->num; index++) {
    //    printf("Initing index: %d\n", index);
    if (pipe(rg->replicas[index].pipefd_into_rep) == -1) {
      perror("replicas pipe error!");
      return 0;
    }

    if (pipe(rg->replicas[index].pipefd_outof_rep) == -1) {
      printf("replicas pipe error!");
      return 0;
    }

    rg->replicas[index].pid = -1;
    rg->replicas[index].priority = -1;
    //    rg->replicas[index].last_result = NULL;
    rg->replicas[index].status = RUNNING;
  }

  // TODO: errors?
  return 1;
}

int forkSingleReplicaNoFD(struct replica_group* rg, int num, char* prog_name) {
  pid_t currentPID = 0;
  char* rep_argv[] = {prog_name, NULL};

  // TODO: Check if replica_group has been inited.
  // Fork child
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      if (-1 == execv(prog_name, rep_argv)) {
	perror("EXEC ERROR!");
	return -1;
      }
    } else { // Parent Process
      rg->replicas[num].pid = currentPID;
    }
  } else {
    printf("Fork error!\n");
    return -1;
  }
}


int forkSingleReplica(struct replica_group* rg, int num, char* prog_name) {
  pid_t currentPID = 0;
  char write_out[3]; // File descriptor rep will write to. Should survive exec()
  char read_in[3];
  char* rep_argv[] = {prog_name, read_in, write_out, NULL};

  // TODO: Check if replica_group has been inited.
  // Fork child
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // art_pot expects something like: ./art_pot read_fd write_fd
      sprintf(read_in, "%02d", rg->replicas[num].pipefd_into_rep[0]);
      rep_argv[1] = read_in;
      sprintf(write_out, "%02d", rg->replicas[num].pipefd_outof_rep[1]);
      rep_argv[2] = write_out;

      close(rg->replicas[num].pipefd_outof_rep[0]); // close read end of outof_rep pipe in child
      close(rg->replicas[num].pipefd_into_rep[1]); // close write end of into_rep pipe in child

      if (-1 == execv(prog_name, rep_argv)) {
	perror("EXEC ERROR!");
	return -1;
      }
    } else { // Parent Process
      close(rg->replicas[num].pipefd_into_rep[0]); // close read end of into_rep pipe in parent
      close(rg->replicas[num].pipefd_outof_rep[1]); // close write end of outof_rep pipe in parent
      rg->replicas[num].pid = currentPID;
    }
  } else {
    printf("Fork error!\n");
    return -1;
  }
}

void replicaCrash(struct replica_group* rg, pid_t pid) {
  int index;

  kill(pid, SIGKILL);
  for (index = 0; index < rg->num; index++) {
    if (rg->replicas[index].pid == pid) {
      rg->replicas[index].status = CRASHED;
    }
  }
}
