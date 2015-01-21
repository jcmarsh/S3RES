#include "../include/replicas.h"
#include <string.h>
#include <stdlib.h>

#define ARGV_REQ 2 // Every controller has it's own name, followed by priority, then all the pipes

void initReplicas(struct replica reps[], int rep_num, const char* name, int priority) {  
  // Init three replicas
  for (int index = 0; index < rep_num; index++) {
    struct replica* new_rep = &reps[index];
    if (new_rep->name != NULL) {
      free(new_rep->name);
    }
    new_rep->name = (char*) malloc(strlen(name) + 1);
    memcpy(new_rep->name, name, strlen(name) + 1);
    new_rep->pid = -1;
    new_rep->priority = priority;
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

/*
 * See forkReplicasSpecial below. Plumber is a special case.
 */
void createPipesSpecial(struct replica reps[], int rep_num, struct typed_pipe ext_pipes[], int pipe_count) {
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

void createPipes(struct replica reps[], int rep_num, struct typed_pipe ext_pipes[], int pipe_count) {
  // external pipes are the pipes for the voter (normally the reps pipes)
  for (int index = 0; index < rep_num; index++) {
    for (int p_index = 0; p_index < pipe_count; p_index++) {
      int pipe_fds[2];
      if (pipe2(pipe_fds, O_CLOEXEC) == -1) { // Need to check on the number of open file descriptors if this line is removed.
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

/*
 * Used only for launching the plumber, which needs to have the pipes sent as arguments.
 * Could have BenchMarker run an fd server for Plumber to remove this special case.
 * OR have a fork_with_args type function. Hmm.
 * TODO: fork_with_args?
 */
void forkReplicasSpecial(struct replica replicas[], int rep_num) {
  for (int index = 0; index < rep_num; index++) {
    // Each replica needs to build up it's argvs
    struct replica* curr = &replicas[index];
    int rep_argc = ARGV_REQ + curr->pipe_count + 1; // 0 is the program name, 1 is the priority, and 1 more for a NULL at the end
    char** rep_argv = (char**)malloc(sizeof(char *) * rep_argc);

    rep_argv[0] = curr->name;
    if (asprintf(&(rep_argv[1]), "%d", curr->priority) < 0) {
      perror("Fork Replica failed arg write");
    }
    for (int a_index = ARGV_REQ; a_index < curr->pipe_count + ARGV_REQ; a_index++) {
      rep_argv[a_index] = serializePipe(curr->rep_pipes[a_index - ARGV_REQ]);
    }
    rep_argv[rep_argc - 1] = NULL;
    curr->pid = forkSingle(rep_argv);
    for (int a_index = 1; a_index < rep_argc; a_index++) {
      free(rep_argv[a_index]);
    }
    free(rep_argv);
  }
}


void forkReplicas(struct replica replicas[], int rep_num) {
  for (int index = 0; index < rep_num; index++) {
    // Each replica needs to build up it's argvs
    struct replica* curr = &replicas[index];
    int rep_argc = ARGV_REQ + 1;    
    char** rep_argv = (char**)malloc(sizeof(char *) * rep_argc);

    rep_argv[0] = curr->name;
    if (asprintf(&(rep_argv[1]), "%d", curr->priority) < 0) {
      perror("Fork Replica failed arg write");
    }
    rep_argv[rep_argc - 1] = NULL;
    curr->pid = forkSingle(rep_argv);
    for (int a_index = 1; a_index < rep_argc; a_index++) {
      free(rep_argv[a_index]);
    }
    free(rep_argv);
  }
}
