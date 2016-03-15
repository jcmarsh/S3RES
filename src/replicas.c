#include "../include/replicas.h"
#include "../include/controller.h"

#ifdef TIME_RESTART_SIGNAL
#include "../tas_lib/inc/tas_time.h"
#endif // TIME_RESTART_SIGNAL

#define ARGV_REQ 3 // Every controller has it's own name, followed by priority and pipe_num, then all the pipes

void initReplicas(struct replica reps[], int rep_num, const char* name, int priority) {  
  int index, jndex;

  for (index = 0; index < rep_num; index++) {
    struct replica* new_rep = &reps[index];
    if (new_rep->name != NULL) {
      free(new_rep->name);
    }
    new_rep->name = (char*) malloc(strlen(name) + 1);
    memcpy(new_rep->name, name, strlen(name) + 1);
    new_rep->pid = -1;
    new_rep->priority = priority;
    if (CONTROLLER_PIN == QUAD_PIN_POLICY) {
      new_rep->pinned_cpu = index + 1;
    } else {
      new_rep->pinned_cpu = CONTROLLER_PIN;
    }

    // clean up pipes if this replica is not fresh
    for (jndex = 0; jndex < new_rep->pipe_count; jndex++) {
      // new_rep->voted[jndex] = 0;
      if (new_rep->pipe_count != 0) {
        resetVotePipe(&(new_rep->vot_pipes[jndex]));
        resetVotePipe(&(new_rep->rep_pipes[jndex]));
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

int bytesReady(struct replica reps[], int num, int pipe_num) {
  int r_index, min = MAX_VOTE_PIPE_BUFF;
  for (r_index = 0; r_index < num; r_index++) {
    if (reps[r_index].vot_pipes[pipe_num].buff_count < min) {
      min = reps[r_index].vot_pipes[pipe_num].buff_count;
    }
  }
  return min;
}

int aheadRep(struct replica reps[], int num, int pipe_num) {
  int r_index = 0;
  int mostAhead = r_index;
  for (r_index = 0; r_index < num; r_index++) {
    //if (reps[r_index].voted[pipe_num] > reps[mostAhead].voted[pipe_num]) {
    if (reps[r_index].vot_pipes[pipe_num].buff_count > reps[mostAhead].vot_pipes[pipe_num].buff_count) {
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
    if (reps[r_index].vot_pipes[pipe_num].buff_count < reps[mostBehind].vot_pipes[pipe_num].buff_count) {
      mostBehind = r_index;
    } else if (reps[r_index].vot_pipes[pipe_num].buff_count == reps[mostBehind].vot_pipes[pipe_num].buff_count) {
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
      gap += reps[aheadRep(reps, num, p_index)].vot_pipes[p_index].buff_count - reps[rep_num].vot_pipes[p_index].buff_count;
    }
  }
  return gap;
}

void restartReplica(struct replica reps[], int num, struct server_data *sd, struct vote_pipe ext_pipes[], int restarter, int restartee, int default_priority) {
  int i, retval, fd_in_c = 0, fd_out_c = 0;

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
        debug_print("Voter error call sched_set_policy in restartReplica for %s, priority %d\n", reps[i].name, priority);
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
  acceptSendFDS(sd, &(reps[restartee].pid), reps[restartee].rep_pipes, reps[restartee].pipe_count, reps[restartee].pinned_cpu);
}

void createPipes(struct replica reps[], int num, struct vote_pipe ext_pipes[], int pipe_count){
  int index, p_index;

  // external pipes are the pipes for the voter (normally the reps pipes)
  for (index = 0; index < num; index++) {
    for (p_index = 0; p_index < pipe_count; p_index++) {
      int pipe_fds[2];
      if (pipe(pipe_fds) == -1) {
      //if (pipe2(pipe_fds, O_CLOEXEC) == -1) { // Need to check on the number of open file descriptors if this line is removed.
        perror("Replica pipe error\n");
      } else {
        struct replica* rep = &reps[index];
        struct vote_pipe ext_pipe = ext_pipes[p_index];

        rep->vot_pipes[p_index].rep_info = ext_pipe.rep_info;
        rep->rep_pipes[p_index].rep_info = ext_pipe.rep_info;
        
        if (ext_pipe.fd_in != 0) { // This pipe is incoming
          rep->vot_pipes[p_index].fd_in = 0;
          rep->vot_pipes[p_index].fd_out = pipe_fds[1];
          rep->rep_pipes[p_index].fd_in = pipe_fds[0];
          rep->voter_rep_in_copy[p_index] = pipe_fds[0];
          rep->rep_pipes[p_index].fd_out = 0;
        } else { // This pipe is outgoing (not friendly)
          rep->vot_pipes[p_index].fd_in = pipe_fds[0];
          rep->vot_pipes[p_index].fd_out = 0;
          rep->rep_pipes[p_index].fd_in = 0;
          rep->voter_rep_in_copy[p_index] = 0;
          rep->rep_pipes[p_index].fd_out = pipe_fds[1];
        }
        
        rep->pipe_count = p_index + 1;
      }
    }
  }
}

void startReplicas(struct replica reps[], int num, struct server_data *sd, const char* name, struct vote_pipe ext_pipes[], int pipe_count, int default_priority) {
  int i;

  initReplicas(reps, num, name, default_priority);
  createPipes(reps, num, ext_pipes, pipe_count);
  forkReplicas(reps, num, 0, NULL);
  for (i = 0; i < num; i++) {
    if (acceptSendFDS(sd, &(reps[i].pid), reps[i].rep_pipes, reps[i].pipe_count,  reps[i].pinned_cpu) < 0) {
      puts("EmptyRestart acceptSendFDS call failed\n");
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
        debug_print("argv[0]: %s\n", argv[0]);
        perror("Replica: EXEC ERROR!");
        return -1;
      }
    } else { // Parent Process
      return currentPID;
    }
  } else {
    perror("Fork error!\n");
    return -1;
  }
}

void forkReplicas(struct replica reps[], int num, int additional_argc, char **additional_argv) {
  int index, a_index;

  for (index = 0; index < num; index++) {
    // Each replica needs to build up it's argvs
    struct replica* curr = &reps[index];
    int rep_argc = ARGV_REQ + additional_argc + 1; // 0 is the program name, 1 is the priority, 2 is the pipe count, and 1 more for a NULL at the end
    char** rep_argv = (char**)malloc(sizeof(char *) * rep_argc);

    rep_argv[0] = curr->name;
    if (asprintf(&(rep_argv[1]), "%d", curr->priority) < 0) {
      perror("Fork Replica failed arg priority write");
    }
    if (asprintf(&(rep_argv[2]), "%d", curr->pipe_count) < 0) {
      perror("Fork Replica failed arg pipe_num write");
    }

    for (a_index = ARGV_REQ; a_index < additional_argc + ARGV_REQ; a_index++) {
      rep_argv[a_index] = additional_argv[a_index - ARGV_REQ];
    }

    rep_argv[rep_argc - 1] = NULL;

    int i;
    debug_print("Args for new replica:\n");
    for (i = 0; i < rep_argc; i++) {
      debug_print("Arg %d: %s\n", i, rep_argv[i]);
    }

    curr->pid = forkSingle(rep_argv);
    for (a_index = 1; a_index < rep_argc; a_index++) {
      free(rep_argv[a_index]);
    }
    free(rep_argv);
  }
}
