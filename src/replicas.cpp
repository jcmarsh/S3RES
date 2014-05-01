#include "../include/replicas.h"

int initReplicas(struct replica_group* rg, struct replica* reps, int num) {
  int index = 0;
  int flags = 0;

  rg->replicas = reps;
  rg->num = num;
  rg->nfds = 0;
  FD_ZERO((&rg->read_fds));
  
  // Init three replicas
  for (index = 0; index < rg->num; index++) {
    //    printf("Initing index: %d\n", index);
    if (pipe(rg->replicas[index].pipefd_into_rep) == -1) {
      printf("Pipe error!\n");
      return 0;
    }

    if (pipe(rg->replicas[index].pipefd_outof_rep) == -1) {
      printf("Pipe error!\n");
      return 0;
    }

    // Need to set to be non-blocking for reading.
    flags = fcntl(rg->replicas[index].pipefd_outof_rep[0], F_GETFL, 0);
    fcntl(rg->replicas[index].pipefd_outof_rep[0], F_SETFL, flags | O_NONBLOCK);
    //    flags = fcntl(rg->replicas[index].pipefd_into_rep[0], F_GETFL, 0);
    //    fcntl(rg->replicas[index].pipefd_into_rep[0], F_SETFL, flags | O_NONBLOCK);

    // nfds should be the highest file descriptor, plus 1
    // TODO: This may have to be changed for when signal fd is added
    if (rg->replicas[index].pipefd_outof_rep[0] >= rg->nfds) {
      rg->nfds = rg->replicas[index].pipefd_outof_rep[0] + 1;
    }
    // Set to select on pipe's file descriptor
    FD_SET(rg->replicas[index].pipefd_outof_rep[0], &(rg->read_fds));

    rg->replicas[index].pid = -1;
    rg->replicas[index].priority = -1;
    //    rg->replicas[index].last_result = NULL;
    rg->replicas[index].status = RUNNING;
  }

  // TODO: errors?
  return 1;
}

// TODO: name of program!
int forkSingleReplica(struct replica_group* rg, int num) {
  pid_t currentPID = 0;
  char write_out[3]; // File descriptor rep will write to. Should survive exec()
  char read_in[3];
  char* rep_argv[] = {"art_pot_p", read_in, write_out, NULL};

  // Fork child
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // art_pot expects something like: ./art_pot 127.0.0.1 6666 2
      // 2 matches the interface index in the .cfg file
      sprintf(read_in, "%02d", rg->replicas[num].pipefd_into_rep[0]);
      rep_argv[1] = read_in;
      sprintf(write_out, "%02d", rg->replicas[num].pipefd_outof_rep[1]);
      rep_argv[2] = write_out;
      if (-1 == execv("art_pot_p", rep_argv)) {
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

void replicaCrash(struct replica_group* rg, pid_t pid) {
  int index;

  kill(pid, SIGKILL);
  for (index = 0; index < rg->num; index++) {
    if (rg->replicas[index].pid == pid) {
      rg->replicas[index].status = CRASHED;
    }
  }
}
