/*
 * Voter intended only for RT components (strict priority, tighter timeouts, possible protection measures).
 * This variant can handle replicas running in parallel
 *
 * Author - James Marshall
 */

#include "voterm.h"

// TODO: Duplicated from controller.h
#define TIMEOUT_SIGNAL 35 // The voter's watchdog timer
#define RESTART_SIGNAL 36 // Voter to replica signal to fork itself

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

void startReplicas(bool forking, int rep_index, int rep_count);

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
          debug_print("VoterM - read on external pipe error - Controller %s pipe %d\n", controller_name, p_index);
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
  bool timeout_occurred = false;
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
      debug_print("Voter writeBuffer failed.\n");
    }
  }

  bool done = false;
  int rep_done = 0;
  while(!done) {
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
    } else { // Timeout
      timeout_occurred = true;
      break; // while(!done)
    }

    retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    for (r_index = 0; r_index < rep_count; r_index++) {
      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        if (FD_ISSET(replicas[r_index].fd_outs[p_index], &select_set)) {
          // TODO: Need to deal with different pipes being timed?
          replicas[r_index].buff_counts[p_index] = read(replicas[r_index].fd_outs[p_index], replicas[r_index].buffers[p_index], MAX_PIPE_BUFF);

          // If the non-timed pipe outputs multiple times, only the last will be saved.
          if (replicas[r_index].buff_counts[p_index] <= 1) {
            debug_print("Voter - read problem on internal pipe - Controller %s, rep %d, pipe %d\n", controller_name, r_index, p_index);
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

  vote(timeout_occurred);
}

// This function will have to deal with some reps having failed (from sdc, or timeout)
void vote(bool timeout_occurred) {
  // Should check for all available output, vote on each and send.
  int p_index;

  switch (rep_count) {
    case 1: // SMR
      if (!timeout_occurred) {
        for (p_index = 0; p_index < out_pipe_count; p_index++) {
          if (replicas[0].buff_counts[p_index] != 0) {
            if (write(ext_out_fds[p_index], replicas[0].buffers[p_index], replicas[0].buff_counts[p_index]) != -1) {
              replicas[0].buff_counts[p_index] = 0;
            } else {
              debug_print("VoterM write failed.\n");
            }
          }
        }
      } else {
        debug_print("Restarting SMR component: %s\n", controller_name);
        for (p_index = 0; p_index < out_pipe_count; p_index++) {
          replicas[0].buff_counts[p_index] = 0;
        }

        startReplicas(true, 0, rep_count);
      }
      return;
    case 2: // DMR
      // Can detect, and check what to do
      // if buff counts don't match: timeout or exec error... unless sdc caused one rep to output

      // if contents don't match: sdc
      debug_print("VoterM Does not handle DMR.\n");
      return;
    case 3: ;// TMR
      int restarter, restartee;
      // Send the solution that at least two agree on
      // TODO: Should stop searching for faults once one is found
      bool fault = false;
      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        if (replicas[0].buff_counts[p_index] != replicas[1].buff_counts[p_index]) {
          if (replicas[0].buff_counts[p_index] != replicas[2].buff_counts[p_index]) {
            fault = true;
            restartee = 0;
          } else {
            fault = true;
            restartee = 1;
          }
        } else if(replicas[0].buff_counts[p_index] != replicas[2].buff_counts[p_index]) {
          fault = true;
          restartee = 2;
        }
      }

      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        if (memcmp(replicas[0].buffers[p_index], replicas[1].buffers[p_index], replicas[0].buff_counts[p_index]) != 0) {
          if (memcmp(replicas[0].buffers[p_index], replicas[2].buffers[p_index], replicas[0].buff_counts[p_index]) != 0) {
            fault = true;
            restartee = 0;
          } else {
            fault = true;
            restartee = 1;
          }
        } else if (memcmp(replicas[0].buffers[p_index], replicas[2].buffers[p_index], replicas[0].buff_counts[p_index]) != 0) {
          fault = true;
          restartee = 2;
        }
      }

      if (!fault) {
        for (p_index = 0; p_index < out_pipe_count; p_index++) {
          if (replicas[0].buff_counts[p_index] != 0) {
            if (write(ext_out_fds[p_index], replicas[0].buffers[p_index], replicas[0].buff_counts[p_index]) != -1) {
            } else {
              debug_print("VoterM write failed.\n");
            }
          }
        }
      } else {
        debug_print("VoterM trying to handle TMR recovery. Timeout? %d\n", timeout_occurred);
        restarter = (restartee + (rep_count - 1)) % rep_count;
        debug_print("\tRestartee: %d - %d\t Restarter: %d - %d\n", restartee, replicas[restartee].pid, restarter, replicas[restarter].pid);

        // kill restartee
        kill(replicas[restartee].pid, SIGKILL);
        // kill (signal) restarter
        kill(replicas[restarter].pid, RESTART_SIGNAL);

        // start restartee
        startReplicas(false, restartee, 1);

        // Recovery should be done. Send data.
        for (p_index = 0; p_index < out_pipe_count; p_index++) {
          if (replicas[restarter].buff_counts[p_index] != 0) {
            if (write(ext_out_fds[p_index], replicas[restarter].buffers[p_index], replicas[restarter].buff_counts[p_index]) != -1) {
            } else {
              debug_print("VoterM write failed.\n");
            }
          }
        }
      }

      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        replicas[0].buff_counts[p_index] = 0;
        replicas[1].buff_counts[p_index] = 0;
        replicas[2].buff_counts[p_index] = 0;
      }
    // switch case statement
  }
}

void forkReplicas(int rep_index, int rep_count) {
  int index, jndex;

  for (index = rep_index; index < rep_index + rep_count; index++) {
    // Each replica needs to build up it's argvs
    // 0 is the program name, 1 is the priority, 2 is the pipe count, and 3 is a NULL
    // TODO: No need to rebuild this for SMR restarts
    int rep_argc = 4;
    int str_index;
    char** rep_argv = (char**)malloc(sizeof(char *) * rep_argc);

    rep_argv[0] = controller_name;

    // rep_priority may be + or -, but less than 3 digits.
    rep_argv[1] = (char *) malloc(sizeof(char) * 4); // 3 + null?
    str_index = 0;
    if (replica_priority < 0) {
      rep_argv[1][str_index++] = '-';
    }
    if (replica_priority / 10.0 > 0) {
      rep_argv[1][str_index++] = 48 + (replica_priority / 10);
    }
    rep_argv[1][str_index++] = 48 + (replica_priority % 10);
    rep_argv[1][str_index] = 0;
    debug_print("CONVERTED %d to %s\n", replica_priority, rep_argv[1]);

    // pipe_count will always be positive, and no more than 2 digits.
    rep_argv[2] = (char *) malloc(sizeof(char) * 3); // 2 + null
    str_index = 0;
    if ((in_pipe_count + out_pipe_count) / 10.0 > 0) {
      rep_argv[2][str_index++] = 48 + ((in_pipe_count + out_pipe_count) / 10);
    }
    rep_argv[2][str_index++] = 48 + ((in_pipe_count + out_pipe_count) % 10);
    rep_argv[2][str_index] = 0;
    debug_print("CONVERTED %d to %s\n", (in_pipe_count + out_pipe_count), rep_argv[2]);

    rep_argv[3] = NULL;

    debug_print("Args for new replica:\n");
    debug_print("Arg 0: %s\tArg 1: %s\tArg 2: %s\n", rep_argv[0], rep_argv[1], rep_argv[2]);

    //replicas[index]->pid = forkSingle(rep_argv);
    pid_t currentPID = fork();

    if (currentPID >= 0) { // Successful fork
      if (currentPID == 0) { // Child process
        if (-1 == execv(rep_argv[0], rep_argv)) {
          debug_print("Replica: EXEC ERROR! - argv[0]: %s\n", rep_argv[0]);
        }
      }
    } else {
      debug_print("Fork error!\n");
    }

    for (jndex = 1; jndex < rep_argc; jndex++) {
      free(rep_argv[jndex]);
    }
    free(rep_argv);
  }
}

// Start all replicas (or just restart one)
void startReplicas(bool forking, int rep_index, int rep_count) {
  int index, jndex;
  //createPipes(reps, num, ext_pipes, pipe_count);
  int pipe_fds[2];
  for (index = rep_index; index < rep_index + rep_count; index++) {
    for (jndex = 0; jndex < in_pipe_count; jndex++) {
      if (pipe(pipe_fds) == -1) {
        debug_print("Replica pipe error\n");
      } else {
        replicas[index].fd_ins[jndex] = pipe_fds[1];
        for_reps[index].fd_ins[jndex] = pipe_fds[0];
      }
    }
  }
  for (index = rep_index; index < rep_index + rep_count; index++) {
    for (jndex = 0; jndex < out_pipe_count; jndex++) {
      if (pipe(pipe_fds) == -1) {
        debug_print("Replica pipe error\n");
      } else {
        replicas[index].fd_outs[jndex] = pipe_fds[0];
        for_reps[index].fd_outs[jndex] = pipe_fds[1];
      }
    }
  }

  //forkReplicas(reps, num, 0, NULL);
  if (forking) {
    forkReplicas(rep_index, rep_count);
  }

  // Give the replicas their pipes (same method as restart)
  for (index = rep_index; index < rep_index + rep_count; index++) {
    int pid = acceptSendFDS(&sd, &for_reps[index], rep_info_in, rep_info_out);
    if (pid < 0) {
      debug_print("VoterM's acceptSendFDS call failed\n");
      exit(-1);
    } else {
      replicas[index].pid = pid;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterM(void) {
  replica_priority = voter_priority - VOTER_PRIO_OFFSET;

  // Setup fd server
  createFDS(&sd, controller_name);

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

  startReplicas(true, 0, rep_count);

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
    debug_print("VoterM failed to read rep_count\n");
  }

  voting_timeout = atoi(argv[3]);
  voter_priority = atoi(argv[4]);
  if (voting_timeout == 0) {
    voting_timeout = PERIOD_USEC;
  }

  if (argc < required_args) { // TODO: Why isn't this check first?
    puts("Usage: VoterM <controller_name> <rep_type> <timeout> <priority> <fd_in:fd_out:timed> <...>");
    return -1;
  } else {
    int pipe_count = argc - required_args;
    int *str_lengths;

    str_lengths = (int*)malloc(sizeof(int) * pipe_count);

    // This is not efficient, but only done at startup
    for (i = 0; i < pipe_count; i++) {
      char rep_info[100] = {0};
      int in, out, timed, j;
      // Format is %s:%d:%d:%d ... but trying to get rid of scanf.
      for (j = 0; j < 100; j++) { // dietlibc can't handle:sscanf(argv[i + required_args], "%m[^:]:%d:%d:%d", &rep_info, &in, &out, &timed);
        if (argv[i + required_args][j] == ':') {
          // j should now be the length of the %s.
          str_lengths[i] = j;
          j++; // move past first ':'

          int int_index = 0;
          char pipe_fd_string[10] = {0};
          while(argv[i + required_args][j + int_index] != ':') {
            pipe_fd_string[int_index] = argv[i + required_args][j + int_index];
            int_index++;
          }
          in = atoi(pipe_fd_string);

          break;
        }
      }
      if (0 != in) {
        in_pipe_count++;
      } else {
        out_pipe_count++;
      }
      //free(rep_info);
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
      int in, out, timed, j;

      rep_info = (char *)malloc(sizeof(char) * str_lengths[i]);

      for (j = 0; j < str_lengths[i]; j++) { // dietlibc can't handle:sscanf(argv[i + required_args], "%m[^:]:%d:%d:%d", &rep_info, &in, &out, &timed);
        rep_info[j] = argv[i + required_args][j];
      }


      j++; // move past first ':'

      // parse in
      int int_index = 0;
      char pipe_fd_string[10] = {0};
      while(argv[i + required_args][j + int_index] != ':') {
        pipe_fd_string[int_index] = argv[i + required_args][j + int_index];
        int_index++;
      }
      in = atoi(pipe_fd_string);
      j = j + int_index + 1; // move past second ':'

      // parse out
      int_index = 0;
      memset(pipe_fd_string, 0, sizeof(pipe_fd_string));
      while(argv[i + required_args][j + int_index] != ':') {
        pipe_fd_string[int_index] = argv[i + required_args][j + int_index];
        int_index++;
      }
      out = atoi(pipe_fd_string);
      j = j + int_index + 1; // move past final ':'

      // parse timed (should be a 1 or 0)
      timed = atoi(&(argv[i + required_args][j]));

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
    free(str_lengths);
  }

  return 0;
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initVoterM() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  sleep(1); // TODO: Needed?

  while(1) {
    recvData();
  }

  return 0;
}
