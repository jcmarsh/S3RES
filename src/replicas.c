#include "../include/replicas.h"
#include "../include/controller.h"
#include <string.h>
#include <stdlib.h>

#define ARGV_REQ 3 // Every controller has it's own name, followed by priority and pipe_num, then all the pipes

void initReplicas(struct replica reps[], int rep_num, const char* name, int priority) {  
  int index, jndex;

  // Init three replicas
  for (index = 0; index < rep_num; index++) {
    struct replica* new_rep = &reps[index];
    if (new_rep->name != NULL) {
      free(new_rep->name);
    }
    new_rep->name = (char*) malloc(strlen(name) + 1);
    memcpy(new_rep->name, name, strlen(name) + 1);
    new_rep->pid = -1;
    new_rep->priority = priority;

    // clean up pipes if this replica is not fresh
    for (jndex = 0; jndex < new_rep->pipe_count; jndex++) {
      new_rep->voted[jndex] = 0;
      if (new_rep->pipe_count != 0) {
        resetPipe(&(new_rep->vot_pipes[jndex]));
        resetPipe(&(new_rep->rep_pipes[jndex]));
      }
    }

    new_rep->pipe_count = 0;
  }
}

void cleanupReplica(struct replica reps[], int rep_index) {
  int i;
  // Kill old replica
  kill(reps[rep_index].pid, SIGKILL); // Make sure it is dead.

  // cleanup replica data structure
  for (i = 0; i < reps[rep_index].pipe_count; i++) {
    if (reps[rep_index].vot_pipes[i].fd_in > 0) {
      close(reps[rep_index].vot_pipes[i].fd_in);
    }
    if (reps[rep_index].vot_pipes[i].fd_out > 0) {
      close(reps[rep_index].vot_pipes[i].fd_out);
    }
    if (reps[rep_index].rep_pipes[i].fd_in > 0) {
      close(reps[rep_index].rep_pipes[i].fd_in);
    }
    if (reps[rep_index].rep_pipes[i].fd_out > 0) {
      close(reps[rep_index].rep_pipes[i].fd_out);
    }
  }

  return;
}

int aheadRep(struct replica reps[], int num, int pipe_num) {
  int r_index = 0;
  int mostAhead = r_index;
  for (r_index = 0; r_index < num; r_index++) {
    if (reps[r_index].voted[pipe_num] > reps[mostAhead].voted[pipe_num]) {
      mostAhead = r_index;
    }
  }
  return mostAhead;
}

// return the index of the rep that is furthest behind in voting
int behindRep(struct replica reps[], int num, int pipe_num) {
  int r_index;
  int mostBehind = 0;
  for (r_index = 1; r_index < num; r_index++) {
    if (reps[r_index].voted[pipe_num] < reps[mostBehind].voted[pipe_num]) {
      mostBehind = r_index;
    } else if (reps[r_index].voted[pipe_num] == reps[mostBehind].voted[pipe_num]) {
      if (reps[r_index].priority > reps[mostBehind].priority) {
        mostBehind = r_index;
      }
    }
  }
  return mostBehind;
}

int rep_gap(struct replica reps[], int num, int rep_num) {
  int p_index = 0;
  int gap = 0;
  for (p_index = 0; p_index < reps[0].pipe_count; p_index++) {
    if (reps[0].vot_pipes[p_index].fd_in != 0) { // out from the rep, in to the voter
      gap += reps[aheadRep(reps, num, p_index)].voted[p_index] - reps[rep_num].voted[p_index];
    }
  }
  return gap;
}

void balanceReps(struct replica reps[], int num, int default_priority) {
  int starting = 0; // most behind rep gets data first
  int second = 1; // the most behind might be dead, so second to go is up next
  int index = 0;

  for (index = 0; index < num; index++) {
    if (rep_gap(reps, num, index) > rep_gap(reps, num, starting)) {
      starting = index;
    } else if (rep_gap(reps, num, index) > rep_gap(reps, num, second)) {
      if (index != starting) {
        second = index;
      }
    }
  }

  for (index = 0; index < num; index++) {    
    int priority;
    if (index == starting) {
      priority = default_priority + 2;
    } else if (index == second) {
      priority = default_priority + 1;
    } else {
      priority = default_priority;
    }
    if (sched_set_policy(reps[index].pid, priority) < 0) {
      // Will fail when the replica is already dead.
      //printf("Voter error call sched_set_policy for %s, priority %d, retval: %d\n", reps[index].name, priority);
    } else {
      reps[index].priority = priority;
    }
  }
}

void createPipes(struct replica reps[], int num, struct typed_pipe ext_pipes[], int pipe_count) {
  int index, p_index;

  // external pipes are the pipes for the voter (normally the reps pipes)
  for (index = 0; index < num; index++) {
    for (p_index = 0; p_index < pipe_count; p_index++) {
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
          rep->voter_rep_in_copy[rep->pipe_count] = pipe_fds[0];
          rep->rep_pipes[rep->pipe_count].fd_out = 0;
        } else { // This pipe is outgoing (not friendly)
          rep->vot_pipes[rep->pipe_count].fd_in = pipe_fds[0];
          rep->vot_pipes[rep->pipe_count].fd_out = 0;
          rep->rep_pipes[rep->pipe_count].fd_in = 0;
          rep->voter_rep_in_copy[rep->pipe_count] = 0;
          rep->rep_pipes[rep->pipe_count].fd_out = pipe_fds[1];
        }
        rep->pipe_count++;
      }
    }
  }
}

void restartReplica(struct replica reps[], int num, struct server_data *sd, struct typed_pipe ext_pipes[], int restarter, int restartee, int default_priority) {
  int i, retval;

  cleanupReplica(reps, restartee);

  // Make the restarter the most special of all the replicas
  for (i = 0; i < num; i++) {    
    int priority;
    if (i != restartee) {
      if (i == restarter) {
        priority = default_priority + 2;
      } else {
        priority = default_priority;
      }
      if (sched_set_policy(reps[i].pid, priority) < 0) {
        printf("Voter error call sched_set_policy in restartReplica for %s, priority %d\n", reps[i].name, priority);
        perror("\tperror");        
      } else {
        reps[i].priority = priority;
      }
    }
  }

  #ifdef TIME_RESTART_SIGNAL
    timestamp_t curr_time = generate_timestamp();
    union sigval time_value;
    time_value.sival_ptr = (void *)curr_time;
    retval = sigqueue(reps[restarter].pid, RESTART_SIGNAL, time_value);
  #else
    retval = kill(reps[restarter].pid, RESTART_SIGNAL);
  #endif /* TIME_RESTART_SIGNAL */
  if (retval < 0) {
    perror("VoterD Signal Problem");
  }

  // re-init failed rep, create pipes
  initReplicas(&(reps[restartee]), 1, reps[restarter].name, default_priority);
  createPipes(&(reps[restartee]), 1, ext_pipes, reps[restarter].pipe_count);
  // send new pipe through fd server (should have a request)
  acceptSendFDS(sd, &(reps[restartee].pid), reps[restartee].rep_pipes, reps[restartee].pipe_count);

  balanceReps(reps, num, default_priority);
}

/*
 * See forkReplicasSpecial below. Plumber is a special case.
 */
void createPipesSpecial(struct replica reps[], int num, struct typed_pipe ext_pipes[], int pipe_count) {
  int index, p_index;

  // external pipes are the pipes for the voter (normally the reps pipes)
  for (index = 0; index < num; index++) {
    for (p_index = 0; p_index < pipe_count; p_index++) {
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
          rep->voter_rep_in_copy[rep->pipe_count] = pipe_fds[0]; // TODO: Necessary?
          rep->rep_pipes[rep->pipe_count].fd_out = 0;
        } else { // This pipe is outgoing (not friendly)
          rep->vot_pipes[rep->pipe_count].fd_in = pipe_fds[0];
          rep->vot_pipes[rep->pipe_count].fd_out = 0;
          rep->rep_pipes[rep->pipe_count].fd_in = 0;
          rep->voter_rep_in_copy[rep->pipe_count] = 0; // TODO: Necessary?
          rep->rep_pipes[rep->pipe_count].fd_out = pipe_fds[1];
        }
        rep->pipe_count++;
      }
    }
  }
}

void startReplicas(struct replica reps[], int num, struct server_data *sd, const char* name, struct typed_pipe ext_pipes[], int pipe_count, int default_priority) {
  int i;
  initReplicas(reps, num, name, default_priority);
  createPipes(reps, num, ext_pipes, pipe_count);
  forkReplicas(reps, num);
  for (i = 0; i < num; i++) {
    if (acceptSendFDS(sd, &(reps[i].pid), reps[i].rep_pipes, reps[i].pipe_count) < 0) {
      printf("EmptyRestart acceptSendFDS call failed\n");
      exit(-1);
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
void forkReplicasSpecial(struct replica reps[], int num) {
  int index, a_index;

  for (index = 0; index < num; index++) {
    // Each replica needs to build up it's argvs
    struct replica* curr = &reps[index];
    int rep_argc = ARGV_REQ + curr->pipe_count + 1; // 0 is the program name, 1 is the priority, and 1 more for a NULL at the end
    char** rep_argv = (char**)malloc(sizeof(char *) * rep_argc);

    rep_argv[0] = curr->name;
    if (asprintf(&(rep_argv[1]), "%d", curr->priority) < 0) {
      perror("Fork Replica Special failed arg priority write");
    }
    if (asprintf(&(rep_argv[2]), "%d", curr->pipe_count) < 0) {
      perror("Fork Replica Special failed arg pipe_num write");
    }
    for (a_index = ARGV_REQ; a_index < curr->pipe_count + ARGV_REQ; a_index++) {
      rep_argv[a_index] = serializePipe(curr->rep_pipes[a_index - ARGV_REQ]);
    }
    rep_argv[rep_argc - 1] = NULL;
    curr->pid = forkSingle(rep_argv);
    for (a_index = 1; a_index < rep_argc; a_index++) {
      free(rep_argv[a_index]);
    }
    free(rep_argv);
  }
}


void forkReplicas(struct replica reps[], int num) {
  int index, a_index;

  for (index = 0; index < num; index++) {
    // Each replica needs to build up it's argvs
    struct replica* curr = &reps[index];
    int rep_argc = ARGV_REQ + 1;    
    char** rep_argv = (char**)malloc(sizeof(char *) * rep_argc);

    rep_argv[0] = curr->name;
    if (asprintf(&(rep_argv[1]), "%d", curr->priority) < 0) {
      perror("Fork Replica failed arg priority write");
    }
    if (asprintf(&(rep_argv[2]), "%d", curr->pipe_count) < 0) {
      perror("Fork Replica failed arg pipe_num write");
    }
    rep_argv[rep_argc - 1] = NULL;
    curr->pid = forkSingle(rep_argv);
    for (a_index = 1; a_index < rep_argc; a_index++) {
      free(rep_argv[a_index]);
    }
    free(rep_argv);
  }
}
