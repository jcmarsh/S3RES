#include "../include/replicas.h"
#include <string.h>
#include <stdlib.h>

// TODO: voting state?

void initReplicas(struct replica reps[], int rep_num, char* name) {  
  // Init three replicas
  for (int index = 0; index < rep_num; index++) {
    struct replica* new_rep = &reps[index];
    new_rep->name = name;
    new_rep->pid = -1;
    new_rep->priority = -1;
    new_rep->status = RUNNING;

    // clean up pipes if this replica is not fresh
    if (new_rep->pipe_count != 0) {
      for (int i = 0; i < new_rep->pipe_count; i++) {
        resetPipe(&(new_rep->vot_pipes[i]));
        resetPipe(&(new_rep->rep_pipes[i]));
      }
    }

    new_rep->pipe_count = 0;
  }
}

void createPipes(struct replica reps[], int rep_num, struct typed_pipe ext_pipes[], int pipe_count) {
  // external pipes are the pipes for the voter (normally the reps pipes)
  for (int index = 0; index < rep_num; index++) {
    for (int p_index = 0; p_index < pipe_count; p_index++) {
      int pipe_fds[2];
      if (pipe(pipe_fds) == -1) {
        printf("Replica pipe error\n");
      } else {
        struct replica* rep = &reps[index];
        struct typed_pipe ext_pipe = ext_pipes[p_index];

        rep->vot_pipes[rep->pipe_count].type = ext_pipe.type;
        rep->rep_pipes[rep->pipe_count].type = ext_pipe.type;

        if (ext_pipe.fd_in != 0) { // This pipe is incoming
          rep->vot_pipes[rep->pipe_count].fd_in = 0;
          rep->vot_pipes[rep->pipe_count].fd_out = pipe_fds[1];
          rep->rep_pipes[rep->pipe_count].fd_in = pipe_fds[0];
          rep->rep_pipes[rep->pipe_count].fd_out = 0;
        } else { // This pipe is outgoing (not friendly)
          rep->vot_pipes[rep->pipe_count].fd_in = pipe_fds[0];
          rep->vot_pipes[rep->pipe_count].fd_out = 0;
          rep->rep_pipes[rep->pipe_count].fd_in = 0;
          rep->rep_pipes[rep->pipe_count].fd_out = pipe_fds[1];
        }
        rep->pipe_count++;
      }
    }
  }
}

/*
 * argv - 0th is the program name, the last is NULL
 * returns the new process' pid
 */
int forkSingle(char** argv) {
  pid_t currentPID = 0;

  // Fork child
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      if (-1 == execv(argv[0], argv)) {
        printf("argv[0]: %s\n", argv[0]);
        perror("Replica: EXEC ERROR!");
        return -1;
      }
    } else { // Parent Process
      return currentPID;
    }
  } else {
    printf("Fork error!\n");
    return -1;
  }
}

void forkReplicas(struct replica replicas[], int rep_num) {
  for (int index = 0; index < rep_num; index++) {
    // Each replica needs to build up it's argvs
    struct replica* curr = &replicas[index];
    int rep_argc = 2 + curr->pipe_count;
    char** rep_argv = (char**)malloc(sizeof(char *) * rep_argc);

    rep_argv[0] = curr->name;
    for (int a_index = 1; a_index <= curr->pipe_count; a_index++) {
      rep_argv[a_index] = serializePipe(curr->rep_pipes[a_index - 1]);
    }
    rep_argv[rep_argc - 1] = NULL;
    curr->pid = forkSingle(rep_argv);
  }
}
