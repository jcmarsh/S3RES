#include "replicas.h"

int initReplicas(struct replica* replicas, int num) {
  int index = 0;
  int flags = 0;

  // Init three replicas
  for (index = 0; index < num; index++) {
    //    printf("Initing index: %d\n", index);
    if (pipe(replicas[index].pipefd) == -1) {
      printf("Pipe error!\n");
      return 1;
    }

    // Need to set to be non-blocking for reading.
    flags = fcntl(replicas[index].pipefd[0], F_GETFL, 0);
    fcntl(replicas[index].pipefd[0], F_SETFL, flags | O_NONBLOCK);

    // nfds should be the highes file descriptor, plus 1
    // TODO: This may have to be changed for when signal fd is added
    if (replicas[index].pipefd[0] >= nfds) {
      nfds = replicas[index].pipefd[0] + 1;
    }
    // Set to select on pipe's file descriptor
    FD_SET(replicas[index].pipefd[0], &read_fds);

    replicas[index].pid = -1;
    replicas[index].priority = -1;
    replicas[index].last_result = 0;
    replicas[index].status = RUNNING;
  }
}

void replicaCrash(pid_t pid) {
  int index;

  kill(pid, SIGKILL);
  for (index = 0; index < CHILD_NUM; index++) {
    if (replicas[index].pid == pid) {
      replicas[index].status = CRASHED;
    }
  }
}

// TODO: Belongs here?
int launchChildren(struct replica* replicas, int num) {
  pid_t currentPID = 0;
  // Fork children
  for (index = 0; index < num; index++) {
    currentPID = fork();
    
    if (currentPID >= 0) { // Successful fork
      if (currentPID == 0) { // Child process
	isChild = 1; // ??????
	write_out = replicas[index].pipefd[1]; // ??????
	break;
      } else { // Parent Process
	replicas[index].pid = currentPID;
      }
    } else {
      printf("Fork error!\n");
      return 1;
    }
  }

  // TODO: Errors
  return 0;
}
