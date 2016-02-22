/*
 * Voter intended only for RT components (strict priority, tighter timeouts, possible protection measures).
 * This variant can handle replicas running in parallel
 *
 * Author - James Marshall
 */

#include "voterr.h"

long voting_timeout;
int timer_start_index; // TODO: Should be per pipe...
int timer_stop_index;
timestamp_t watchdog;

// Replica related data
struct replicaR *replicas;
// The replica half of the data
struct replicaR *for_reps;

// TAS Stuff
int voter_priority;
int replica_priority;

// FD server
struct server_data sd;

int rep_count;
char* controller_name;

// pipes to external components (not replicas)
// "in" means going into the voter from external, or into the replica from the voter
// "out" means going out from the voter to external, or out from the replica to the voter
int in_pipe_count = 0; // The replicaR structure duplicates pipe counts.
int out_pipe_count = 0;
unsigned int * ext_in_fds; // should in_pipe_count of these
char **rep_info_in;
int ext_in_bufcnt = 0;
unsigned char ext_in_buffer[MAX_PIPE_BUFF];
unsigned int * ext_out_fds; // should out_pipe_count of these
char **rep_info_out;

void recvData(void) {
  int p_index;
  int retval = 0;

  struct timeval select_timeout;
  fd_set select_set;

  select_timeout.tv_sec = 0;
  select_timeout.tv_usec = 50000;

  // See if any of the read pipes have anything
  FD_ZERO(&select_set);
  for (p_index = 0; p_index < in_pipe_count; p_index++) {
    FD_SET(ext_in_fds[p_index], &select_set);
  }

  // This will wait at least timeout until return. Returns earlier if something has data.
  retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);

  int active_index = -1;
  if (retval > 0) {    
    // Check for data from external sources
    for (p_index = 0; p_index < in_pipe_count; p_index++) {
      if (FD_ISSET(ext_in_fds[p_index], &select_set)) {
        ext_in_bufcnt = read(ext_in_fds[p_index], ext_in_buffer, MAX_PIPE_BUFF);
        if (ext_in_bufcnt > 0) {
          active_index = p_index;
          break;
        } else {
          printf("Voter - Controller %s pipe %d\n", controller_name, p_index);
          perror("Voter - read on external pipe error");
        }
      }
    }
  }

  if (-1 == active_index) {
    return; // Will loop back to start of function
  } else {
    sendCollect(active_index);
  }
}

void sendCollect(int active_index) {
  int p_index, r_index, retval;
  struct replicaR *c_rep;
  fd_set select_set;
  struct timeval select_timeout;

  // Start timer for the individual replica.
  // TODO: This should be a pipe specific timer
  if (active_index == timer_start_index) {
    watchdog = generate_timestamp();
  }

  // send data to all replicas
  for (r_index = 0; r_index < rep_count; r_index++) {
    retval = write(replicas[r_index].fd_ins[active_index], ext_in_buffer, ext_in_bufcnt);
    if (retval != ext_in_bufcnt) {
      perror("Voter writeBuffer failed.");
    }
  }

  bool done = false;
  int rep_done = 0;
  while (!done) {
    // Select, but only over outgoing pipes from the replicas
    FD_ZERO(&select_set);
    for (r_index = 0; r_index < rep_count; r_index++) {
      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        FD_SET(replicas[r_index].fd_outs[p_index], &select_set);
      }
    }

    timestamp_t current = generate_timestamp();
    long remaining = voting_timeout - diff_time(current, watchdog, CPU_MHZ);
    if (remaining > 0) {
      select_timeout.tv_sec = 0;
      select_timeout.tv_usec = remaining;
    } else {
      // Timeout, should be detected by voting.
      break; // while(!done)
    }

    retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    for (r_index = 0; r_index < rep_count; r_index++) {
      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        if (FD_ISSET(c_rep->fd_outs[p_index], &select_set)) {
          // TODO: Need to deal with different pipes being timed?

          c_rep->buff_counts[p_index] = read(c_rep->fd_outs[p_index], c_rep->buffers[p_index], MAX_PIPE_BUFF);

          // If the non-timed pipe outputs multiple times, only the last will be saved.
          if (c_rep->buff_counts[p_index] <= 1) {
            printf("Voter - Controller %s, rep %d, pipe %d\n", controller_name, r_index, p_index);
            perror("Voter - read problem on internal pipe");
          }

          if (p_index == timer_stop_index) {
            rep_done++;
            if (rep_done == rep_count) {
              done = true; // All timed pipe calls are in. Off to voting.
            }
          }
        } // if FD_ISSET
      } // for pipe
    } //for replica
  } // while !done

  vote();
}

// This function will have to deal with some reps having failed (from sdc, or timeout)
void vote() {
  // Should check for all available output, vote on each and send.
  int p_index;

  switch (rep_count) {
    case 1: // SMR
      printf("VoterM Does not handle SMR.\n");
      return;
    case 2: // DMR
      // Can detect, and check what to do
      // if buff counts don't match: timeout or exec error... unless sdc caused one rep to output

      // if contents don't match: sdc
      printf("VoterM Does not handle DMR.\n");
      return;
    case 3: ;// TMR
      // Send the solution that at least two agree on
      bool fault = false;
      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        if ((replicas[0].buff_counts[p_index] != replicas[1].buff_counts[p_index])
          || (replicas[0].buff_counts[p_index] != replicas[2].buff_counts[p_index])) {
          fault = true;
        }
      }

      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        if ((memcmp(replicas[0].buffers[p_index], replicas[1].buffers[p_index], replicas[0].buff_counts[p_index]) != 0)
          || (memcmp(replicas[0].buffers[p_index], replicas[2].buffers[p_index], replicas[0].buff_counts[p_index]) != 0)) {
          fault = true;
        }
      }

      if (!fault) {
        for (p_index = 0; p_index < out_pipe_count; p_index++) {
          if (replicas[0].buff_counts[p_index] != 0) {
            if (write(ext_out_fds[p_index], replicas[0].buffers[p_index], replicas[0].buff_counts[p_index]) != -1) {
              replicas[0].buff_counts[p_index] = 0;
              replicas[1].buff_counts[p_index] = 0;
              replicas[2].buff_counts[p_index] = 0;
            } else {
              perror("VoterM write failed");
            }
          }
        }
      } else {
        printf("VoterM does not handle TMR recovery.\n");
      }
    // switch case statement
  }
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterD(void) {
  replica_priority = voter_priority - VOTER_PRIO_OFFSET;

  // Setup fd server
  createFDS(&sd, controller_name);

  // TODO: This is the last piece to convert! And likely the hardest.
  //startReplicas(replicas, rep_count, &sd, controller_name, ext_pipes, pipe_count, replica_priority);
  //initReplicas(reps, num, name, default_priority);
  int index, jndex;
  replicas = (struct replicaR *) malloc(sizeof(struct replicaR) * rep_count);
  for_reps = (struct replicaR *) malloc(sizeof(struct replicaR) * rep_count);
  for (index = 0; index < rep_count; index++) {
    if (CONTROLLER_PIN == QUAD_PIN_POLICY) {
      replicas[index].pinned_cpu = index + 1; // TODO: don't need both
      for_reps[index].pinned_cpu = index + 1;
    } else {
      replicas[index].pinned_cpu = CONTROLLER_PIN; // TODO: don't need both
      for_reps[index].pinned_cpu = CONTROLLER_PIN;
    }

    replicas[index].in_pipe_count = in_pipe_count; // Duplicated to make fds data passing easier
    for_reps[index].in_pipe_count = in_pipe_count;
    replicas[index].fd_ins = (unsigned int *) malloc(sizeof(int) * in_pipe_count);
    for_reps[index].fd_ins = (unsigned int *) malloc(sizeof(int) * in_pipe_count);
    replicas[index].out_pipe_count = out_pipe_count; // Duplicated to make fds data passing easier
    for_reps[index].out_pipe_count = out_pipe_count;
    replicas[index].fd_outs = (unsigned int *) malloc(sizeof(int) * out_pipe_count);
    for_reps[index].fd_outs = (unsigned int *) malloc(sizeof(int) * out_pipe_count);
    replicas[index].buff_counts = (unsigned int *) malloc(sizeof(int) * out_pipe_count);
    replicas[index].buffers = (unsigned char **) malloc(sizeof(char*) * out_pipe_count);
    for (jndex = 0; jndex < out_pipe_count; jndex++) {
      replicas[index].buffers[jndex] = (unsigned char *) malloc(sizeof(char) * MAX_PIPE_BUFF);
    }
  }


  //createPipes(reps, num, ext_pipes, pipe_count);
  int pipe_fds[2];
  for (index = 0; index < rep_count; index++) {
    for (jndex = 0; jndex < in_pipe_count; jndex++) {
      if (pipe(pipe_fds) == -1) {
        printf("Replica pipe error\n");
      } else {
        replicas[index].fd_ins[jndex] = pipe_fds[1];
        for_reps[index].fd_ins[jndex] = pipe_fds[0];
      }
    }
  }
  for (index = 0; index < rep_count; index++) {
    for (jndex = 0; jndex < out_pipe_count; jndex++) {
      if (pipe(pipe_fds) == -1) {
        printf("Replica pipe error\n");
      } else {
        replicas[index].fd_outs[jndex] = pipe_fds[0];
        for_reps[index].fd_outs[jndex] = pipe_fds[1];
      }
    }
  }

  //forkReplicas(reps, num, 0, NULL);
  for (index = 0; index < rep_count; index++) {
    // Each replica needs to build up it's argvs
    // 0 is the program name, 1 is the priority, 2 is the pipe count, and 3 is a NULL
    int rep_argc = 4;
    char** rep_argv = (char**)malloc(sizeof(char *) * rep_argc);

    rep_argv[0] = controller_name;
    if (asprintf(&(rep_argv[1]), "%d", replica_priority) < 0) {
      perror("Fork Replica failed arg priority write");
    }
    if (asprintf(&(rep_argv[2]), "%d", in_pipe_count + out_pipe_count) < 0) {
      perror("Fork Replica failed arg pipe_num write");
    }

    rep_argv[3] = NULL;

    debug_print("Args for new replica:\n");
    debug_print("Arg 0: %s\tArg 1: %s\tArg 2: %s\n", rep_argv[0], rep_argv[1], rep_argv[2]);

    //replicas[index]->pid = forkSingle(rep_argv);
    pid_t currentPID = fork();

    if (currentPID >= 0) { // Successful fork
      if (currentPID == 0) { // Child process
        if (-1 == execv(rep_argv[0], rep_argv)) {
          printf("argv[0]: %s\n", rep_argv[0]);
          perror("Replica: EXEC ERROR!");
          return -1;
        }
      }
    } else {
      printf("Fork error!\n");
    }


    for (jndex = 1; jndex < rep_argc; jndex++) {
      free(rep_argv[jndex]);
    }
    free(rep_argv);
  }

  // Give the replicas their pipes (same method as restart)
  for (jndex = 0; jndex < rep_count; jndex++) {
    if (acceptSendFDS(&sd, &for_reps[jndex], rep_info_in, rep_info_out, for_reps[jndex].pinned_cpu) < 0) {
      printf("EmptyRestart acceptSendFDS call failed\n");
      exit(-1);
    }
  }

  InitTAS(VOTER_PIN, voter_priority); // IMPORTANT: Should be after forking replicas to subvert CoW

  debug_print("Initializing VoterM(%s)\n", controller_name);

  return 0;
}

int parseArgs(int argc, const char **argv) {
  int i;
  int required_args = 5; // voter name, controller name, rep_type, timeout and priority
  controller_name = (char*) (argv[1]);

  if ('S' == argv[2][0]) { // Just checking the first character
    rep_count = 1; // SMR
  } else if ('D' == argv[2][0]) {
    rep_count = 2; // DMR
  } else if ('T' == argv[2][0]) {
    rep_count = 3; // TMR
  } else {
    rep_count = 0;
    printf("VoterM failed to read rep_count\n");
  }

  voting_timeout = atoi(argv[3]);
  voter_priority = atoi(argv[4]);
  if (voting_timeout == 0) {
    voting_timeout = PERIOD_USEC;
  }

  if (argc < required_args) { 
    puts("Usage: VoterD <controller_name> <rep_type> <timeout> <priority> <fd_in:fd_out:timed> <...>");
    return -1;
  } else {
    int pipe_count = argc - required_args;

    // This is not efficient, but only done at startup
    for (i = 0; i < pipe_count; i++) {
      char * rep_info;
      int in, out, timed;
      sscanf(argv[i + required_args], "%m[^:]:%d:%d:%d", &rep_info, &in, &out, &timed);
      if (0 != in) {
        in_pipe_count++;
      } else {
        out_pipe_count++;
      }
      free(rep_info);
    }

    // malloc data structures
    ext_in_fds = (unsigned int *) malloc(sizeof(int) * in_pipe_count);
    rep_info_in = (char **) malloc(sizeof(char*) * in_pipe_count);
    ext_out_fds = (unsigned int *) malloc(sizeof(int) * out_pipe_count);
    rep_info_out = (char **) malloc(sizeof(char*) * out_pipe_count);

    // parse values
    int c_in_pipe = 0;
    int c_out_pipe = 0;
    for (i = 0; i < pipe_count; i++) {
      char * rep_info;
      int in, out, timed;

      sscanf(argv[i + required_args], "%m[^:]:%d:%d:%d", &rep_info, &in, &out, &timed);
      if (0 != in) {
        ext_in_fds[c_in_pipe] = in;
        rep_info_in[c_in_pipe] = rep_info;
        if (timed) {
          timer_start_index = c_in_pipe;
        }
        c_in_pipe++;
      } else {
        ext_out_fds[c_out_pipe] = out;
        rep_info_out[c_out_pipe] = rep_info;
        if (timed) {
          timer_stop_index = c_out_pipe;
        }
        c_out_pipe++;
      }
    }
  }

  return 0;
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initVoterD() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  sleep(1); // TODO: Needed?

  while(1) {
    recvData();
  }

  return 0;
}
