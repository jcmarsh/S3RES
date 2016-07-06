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
#define RRUSAGE_SIGNAL 39 // Report rusage info

long voting_timeout;
// A pair of pipes can be associated (and timed)
int indexed_pipes = 0;
// These hold the indexes into rep_info_in... etc.
int timer_start_index[2]; // Should have a higher max than two...
int timer_stop_index[2];
timestamp_t watchdog;

// Replica related data
struct replicaR *replicas;
// The replica half of the data
struct replicaR *for_reps;

// TAS Stuff
int voter_priority;

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

#ifdef RUSAGE_ENABLE
timestamp_t start_time;
void reportRUsageHandler(int sign, siginfo_t *si, void *unused) {
  // getrusage isn't in the safe list... so we'll see.
  timestamp_t current = generate_timestamp();
  struct rusage usage;

  printf("Report RUsage VoterM(%s) - (%d)\n", controller_name, getpid());
  printf("\tTime since process init: %lf\n", diff_time(current, start_time, CPU_MHZ));

  if (getrusage(RUSAGE_SELF, &usage) < 0) {
    perror("Controller getrusage failed");
  } else {
    printf("\tUtime:\t%ld Sec %ld uSec\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
    printf("\tStime:\t%ld Sec %ld uSec\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);

    printf("\tRSS:\t %ld %ld %ld %ld\n", usage.ru_maxrss, usage.ru_ixrss, usage.ru_idrss, usage.ru_isrss);
    printf("\tFLT:\t %ld %ld\n", usage.ru_minflt, usage.ru_majflt);
  }
  fflush(stdout);
}
#endif /* RUSAGE_ENABLE */

// sets active_pipe_index (pipe data came in on)
int active_pipe_index;
bool recvData(void) {
  int p_index;
  int retval = 0;

  active_pipe_index = -1;

  struct timeval select_timeout;
  fd_set select_set;

  select_timeout.tv_sec = 0;
  select_timeout.tv_usec = 500000;

  // See if any of the read pipes have anything
  FD_ZERO(&select_set);
  for (p_index = 0; p_index < in_pipe_count; p_index++) {
    FD_SET(ext_in_fds[p_index], &select_set);
  }

  // This will wait at least timeout until return. Returns earlier if something has data.
  retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);

  if (retval > 0) {    
    // Check for data from external sources
    for (p_index = 0; p_index < in_pipe_count; p_index++) {
      if (FD_ISSET(ext_in_fds[p_index], &select_set)) {
        ext_in_bufcnt = read(ext_in_fds[p_index], ext_in_buffer, MAX_PIPE_BUFF);
        if (ext_in_bufcnt > 0) {
          active_pipe_index = p_index;
          break;
        } else {
          debug_print("VoterM - read on external pipe error - Controller %s pipe %d\n", controller_name, p_index);
        }
      }
    }
  }

  if (-1 == active_pipe_index) {
    return false; // Will loop back to start of function
  } else {
    return true;
  }
}

bool checkWaitOnInput(void) {
  // If input is on a non-timed pipe (ex. comm_ack into mapper), do not wait for data to be returned.
  int p_index;
  bool waiting = false;
  for (p_index = 0; p_index < indexed_pipes; p_index++) {
    if (active_pipe_index == timer_start_index[p_index]) {
      waiting = true;
    }
  }
  if (!waiting) {
    return false; // Not a timed pipe
  }

  return true;
}

// sends data to replica in pipe[active_pipe_index]
bool sendToReps(void) {
  int p_index, r_index, retval;

  // send data to all replicas
  for (r_index = 0; r_index < rep_count; r_index++) {
    retval = write(replicas[r_index].fd_ins[active_pipe_index], ext_in_buffer, ext_in_bufcnt);
    if (retval != ext_in_bufcnt) {
      debug_print("Voter writeBuffer failed.\n");
    }
  }

  return checkWaitOnInput();
}

// returns number of reps that are done. Set timer and wait for expected responses
bool timeout_occurred;
int  collectFromReps(bool set_timer, int expected) {
  int p_index, r_index, retval;
  fd_set select_set;
  struct timeval select_timeout;
  timeout_occurred = false;
  int rep_done = 0;

  // Timers for ALL replicas with quad core
  watchdog = generate_timestamp();

  bool done = false;
  while(!done) {
    // Select, but only over outgoing pipes from the replicas
    FD_ZERO(&select_set);
    for (r_index = 0; r_index < rep_count; r_index++) {
      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        if (replicas[r_index].fd_outs[p_index] != 0) { // FD may have been closed by killAndClean
          FD_SET(replicas[r_index].fd_outs[p_index], &select_set);
        }
      }
    }

    if (set_timer) {
      timestamp_t current = generate_timestamp();
      long remaining = voting_timeout - diff_time(current, watchdog, CPU_MHZ);
      if (remaining > 0) {
        select_timeout.tv_sec = 0;
        select_timeout.tv_usec = remaining;
      } else { // Timeout detected
        timeout_occurred = true;
        break; // while(!done)
      }
    } else {
      select_timeout.tv_sec = 0;
      select_timeout.tv_usec = 50000;
    }

    retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    for (r_index = 0; r_index < rep_count; r_index++) {
      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        if (FD_ISSET(replicas[r_index].fd_outs[p_index], &select_set)) {
          replicas[r_index].buff_counts[p_index] = read(replicas[r_index].fd_outs[p_index], replicas[r_index].buffers[p_index], MAX_PIPE_BUFF);

          // If the non-timed pipe outputs multiple times, only the last will be saved.
          if (replicas[r_index].buff_counts[p_index] <= 1) {
            debug_print("Voter - read problem on internal pipe - Controller %s, rep %d, pipe %d\n", controller_name, r_index, p_index);
          }

          if (p_index == timer_stop_index[active_pipe_index]) {
            rep_done++;
            if (rep_done == expected) {
              done = true; // All timed pipe calls are in. Off to voting.
            }
          }
        } // if FD_ISSET
      } // for pipe
    } //for replica
  } // while !done

  return rep_done;
}

// A timeout was detected, which rep has not yet responded?
// On quad core this is easy, since only the frozen rep will have failed to respond.
// On a single core system, a high priority rep could have locked up and prevent all other reps from running.
int fault_index;
void findFaultReplica(void) {
  int lowest_score = out_pipe_count; // output on every pipe is max score
  int r_index, p_index;

  fault_index = 0;

  for (r_index = 0; r_index < rep_count; r_index++) {
    int rep_score = 0;
    // each pipe with output is counted as '1', assuming no partial writes (or multiples)
    for (p_index = 0; p_index < out_pipe_count; p_index++) {
      if (replicas[r_index].buff_counts[p_index] > 0) {
        rep_score++;
      }
    }
    if (rep_score < lowest_score) {
      // Replicas with a lower index have a higher priority, so '<' makes priority the tie-breaker.
      fault_index = r_index;
      lowest_score = rep_score;
    }
  }
}

// kill a replica, and clean up its fds. If SMR, wait for its zombie.
void killAndClean(int rep_to_kill) {
  int p_index;

  kill(replicas[rep_to_kill].pid, SIGKILL);

  for (p_index = 0; p_index < in_pipe_count; p_index++) {
    // rep and vote sides
    close(replicas[rep_to_kill].fd_ins[p_index]);
    replicas[rep_to_kill].fd_ins[p_index] = 0;
    close(for_reps[rep_to_kill].fd_ins[p_index]);
    for_reps[rep_to_kill].fd_ins[p_index] = 0;
  }
  for (p_index = 0; p_index < out_pipe_count; p_index++) {
    // rep and vote sides
    close(replicas[rep_to_kill].fd_outs[p_index]);
    replicas[rep_to_kill].fd_outs[p_index] = 0;
    close(for_reps[rep_to_kill].fd_outs[p_index]);
    for_reps[rep_to_kill].fd_outs[p_index] = 0;
  }

  // Prevents zombies from accumulating
  if (1 == rep_count) {
    waitpid(-1, NULL, WNOHANG);
  }
}

// checkSDC will have to kill faulty replicas on its own and set fault_index
bool checkSDC(void) { // returns true if SDC was found
  // Should check for all available output, vote on each and send.
  int p_index;
  int restarter;
  bool fault = false;

  switch (rep_count) {
    case 1: ; // SMR
      break;
    case 2: ; // DMR
      // Can only detect SDCs
      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        if (replicas[0].buff_counts[p_index] != replicas[1].buff_counts[p_index]) {
          debug_print("VoterM(%s) DMR Buff counts off on pipe %d: %d - %d\n", controller_name, p_index, replicas[0].buff_counts[p_index], replicas[1].buff_counts[p_index]);
          fault = true;
          if (replicas[0].buff_counts[p_index] < replicas[1].buff_counts[p_index]) {
            fault_index = 0; // Can't be sure which one, but guess one that hasn't responded yet.
          } else {
            fault_index = 1;
          }
        } else if (memcmp(replicas[0].buffers[p_index], replicas[1].buffers[p_index], replicas[0].buff_counts[p_index]) != 0) {
          debug_print("VoterM(%s) DMR Buff contents off.\n", controller_name);
          fault = true;
          fault_index = 0; // Take a guess, 50 / 50 shot right?
        }
      }
      break;
    case 3: ;// TMR
      // Send the solution that at least two agree on
      // Check buffer counts
      for (p_index = 0; p_index < out_pipe_count; p_index++) {
        if (replicas[0].buff_counts[p_index] != replicas[1].buff_counts[p_index]) {
          if (replicas[0].buff_counts[p_index] != replicas[2].buff_counts[p_index]) {
            debug_print("VoterM(%s) Buff counts off: %d - %d vs %d - %d\n", controller_name, replicas[0].pid, replicas[0].buff_counts[p_index], replicas[1].pid, replicas[1].buff_counts[p_index]);
            fault = true;
            fault_index = 0;
          } else {
            debug_print("VoterM(%s) Buff counts off: %d - %d vs %d - %d\n", controller_name, replicas[0].pid, replicas[0].buff_counts[p_index], replicas[1].pid, replicas[1].buff_counts[p_index]);
            fault = true;
            fault_index = 1;
          }
        } else if(replicas[0].buff_counts[p_index] != replicas[2].buff_counts[p_index]) {
          debug_print("VoterM(%s) Buff counts off: %d - %d vs %d - %d\n", controller_name, replicas[2].pid, replicas[2].buff_counts[p_index], replicas[1].pid, replicas[1].buff_counts[p_index]);
          fault = true;
          fault_index = 2;
        }
      }

      // check buffer contents
      if (!fault) {
        for (p_index = 0; p_index < out_pipe_count; p_index++) {
          if (memcmp(replicas[0].buffers[p_index], replicas[1].buffers[p_index], replicas[0].buff_counts[p_index]) != 0) {
            if (memcmp(replicas[0].buffers[p_index], replicas[2].buffers[p_index], replicas[0].buff_counts[p_index]) != 0) {
              debug_print("VoterM(%s) SDC spotted: %d\n", controller_name, replicas[0].pid);
              fault = true;
              fault_index = 0;
            } else {
              debug_print("VoterM(%s) SDC spotted: %d\n", controller_name, replicas[1].pid);
              fault = true;
              fault_index = 1;
            }
          } else if (memcmp(replicas[0].buffers[p_index], replicas[2].buffers[p_index], replicas[0].buff_counts[p_index]) != 0) {
            debug_print("VoterM(%s) SDC spotted: %d\n", controller_name, replicas[2].pid);
            fault = true;
            fault_index = 2;
          }
        }
      }
      break;
  } // switch(rep_count)

  if (fault) {
    killAndClean(fault_index);
  }
  return fault;
}

// sends data from rep_to_send out of voter. Reset all state for next loop
void sendData(int rep_to_send) {
  int p_index, r_index;

  for (p_index = 0; p_index < out_pipe_count; p_index++) {
    if (replicas[rep_to_send].buff_counts[p_index] != 0) {
      if (write(ext_out_fds[p_index], replicas[rep_to_send].buffers[p_index], replicas[rep_to_send].buff_counts[p_index]) != -1) {
      } else {
        debug_print("VoterM write failed.\n");
      }
    }
  }

  for (r_index = 0; r_index < rep_count; r_index++) {
    for (p_index = 0; p_index < out_pipe_count; p_index++) {
      replicas[r_index].buff_counts[p_index] = 0;
    }
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
    if (replicas[index].priority < 0) {
      rep_argv[1][str_index++] = '-';
    }
    if (replicas[index].priority / 10.0 > 0) {
      rep_argv[1][str_index++] = 48 + (replicas[index].priority / 10);
    }
    rep_argv[1][str_index++] = 48 + (replicas[index].priority % 10);
    rep_argv[1][str_index] = 0;

    // pipe_count will always be positive, and no more than 2 digits.
    rep_argv[2] = (char *) malloc(sizeof(char) * 3); // 2 + null
    str_index = 0;
    if ((in_pipe_count + out_pipe_count) / 10.0 > 0) {
      rep_argv[2][str_index++] = 48 + ((in_pipe_count + out_pipe_count) / 10);
    }
    rep_argv[2][str_index++] = 48 + ((in_pipe_count + out_pipe_count) % 10);
    rep_argv[2][str_index] = 0;
    //debug_print("CONVERTED %d to %s\n", (in_pipe_count + out_pipe_count), rep_argv[2]);

    rep_argv[3] = NULL;

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
void startReplicas(bool forking, int rep_index, int rep_start_count) {
  debug_print("VoterM(%s) is starting a replica.\n", controller_name);

  #ifdef TIME_RESTART_REPLICA
    timestamp_t last = generate_timestamp();
  #endif /* TIME_RESTART_REPLICA */
  int index, jndex;
  //createPipes(reps, num, ext_pipes, pipe_count);
  int pipe_fds[2];
  for (index = rep_index; index < rep_index + rep_start_count; index++) {
    for (jndex = 0; jndex < in_pipe_count; jndex++) {
      if (pipe(pipe_fds) == -1) {
        debug_print("Replica pipe error\n");
      } else {
        replicas[index].fd_ins[jndex] = pipe_fds[1];
        for_reps[index].fd_ins[jndex] = pipe_fds[0];
      }
    }
  }
  for (index = rep_index; index < rep_index + rep_start_count; index++) {
    for (jndex = 0; jndex < out_pipe_count; jndex++) {
      if (pipe(pipe_fds) == -1) {
        debug_print("Replica pipe error\n");
      } else {
        replicas[index].fd_outs[jndex] = pipe_fds[0];
        for_reps[index].fd_outs[jndex] = pipe_fds[1];
      }
    }
  }

  // Give the replicas their pipes (same method as restart)
  for (index = rep_index; index < rep_index + rep_start_count; index++) {
    if (forking) {
      forkReplicas(index, 1); // Fork one at a time.
    } else { // If voter isn't forking, find a replica to instead.
      // rep_start_count should only be 1 in this case
      kill(replicas[(fault_index + (rep_count - 1)) % rep_count].pid, RESTART_SIGNAL);
    }
    int pid = acceptSendFDS(&sd, &for_reps[index], rep_info_in, rep_info_out);
    if (pid < 0) {
      debug_print("VoterM's acceptSendFDS call failed\n");
      exit(-1);
    } else {
      replicas[index].pid = pid;
    }
  }
  #ifdef TIME_RESTART_REPLICA
    timestamp_t current = generate_timestamp();
    printf("voterm_usec restart (%lf)\n", diff_time(current, last, CPU_MHZ));
  #endif /* TIME_RESTART_REPLICA */
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int initVoterM(void) {
#ifdef RUSAGE_ENABLE
  // register for rusage signal and take start time.
  struct sigaction sa;
  start_time = generate_timestamp();

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = reportRUsageHandler;
  if (sigaction(RRUSAGE_SIGNAL, &sa, NULL) == -1) {
    debug_print("Failed to register VoterM for the report rusage handler.\n");
    return -1;
  }
#endif /* RUSAGE_ENABLE */

  // Setup fd server
  // Horrible hack for having different named GenericEmpty controllers for tests
  if (controller_name[0] == 'G' && controller_name[7] == 'E') {
    createFDS(&sd, "GenericEmpty");
  } else {
    createFDS(&sd, controller_name);
  }

  //initReplicas(reps, num, name, default_priority);
  int index, jndex;
  replicas = (struct replicaR *) malloc(sizeof(struct replicaR) * rep_count);
  for_reps = (struct replicaR *) malloc(sizeof(struct replicaR) * rep_count);
  for (index = 0; index < rep_count; index++) {
    // replicas[index].priority = voter_priority - (1 + index);
    replicas[index].priority = voter_priority - 4;
    // for_reps[index].priority = voter_priority - (1 + index);
    for_reps[index].priority = voter_priority - 4;
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

      // parse timed (should be 0 if not timed, index of pair otherwise (usually 1, except for A_Star))
      timed = atoi(&(argv[i + required_args][j]));

      if (0 != in) {
        ext_in_fds[c_in_pipe] = in;
        rep_info_in[c_in_pipe] = rep_info;
        if (timed) {
          indexed_pipes++;
          timer_start_index[timed - 1] = c_in_pipe;
        }
        c_in_pipe++;
      } else {
        ext_out_fds[c_out_pipe] = out;
        rep_info_out[c_out_pipe] = rep_info;
        if (timed) {
          timer_stop_index[timed - 1] = c_out_pipe;
        }
        c_out_pipe++;
      }
    }
    free(str_lengths);
  }

  return 0;
}

// returns -1 if no response expected, # of responses otherwise
int sendCollect() {
  timeout_occurred = false;
  int p_index, r_index, retval, done_count = 0;
  fd_set select_set;
  struct timeval select_timeout;

  // send data to all replicas
  bool waiting = checkWaitOnInput();
  for (r_index = 0; r_index < rep_count; r_index++) {
    retval = write(replicas[r_index].fd_ins[active_pipe_index], ext_in_buffer, ext_in_bufcnt);
    if (retval != ext_in_bufcnt) {
      debug_print("Voter writeBuffer failed.\n");
    }

    if (waiting) {
      watchdog = generate_timestamp();

      // Set rep priority to high
      if (sched_set_policy(replicas[r_index].pid, voter_priority - 1) < 0) {
        // Will fail when the replica is already dead.
        debug_print("sched_set_policy failed: %d, %d\n", replicas[r_index].pid,  voter_priority - 1);
      } else {
        replicas[r_index].priority = voter_priority - 1;
        for_reps[r_index].priority = voter_priority - 1;
      }

      bool done = false;
      while (!done) {
        timestamp_t current = generate_timestamp();
        long remaining = voting_timeout - diff_time(current, watchdog, CPU_MHZ);
        if (remaining > 0) {
          select_timeout.tv_sec = 0;
          select_timeout.tv_usec = remaining;
        } else { // Timeout
          timeout_occurred = true;
          fault_index = r_index;
          break; // while(!done)
        }

        FD_ZERO(&select_set);
        for (p_index = 0; p_index < out_pipe_count; p_index++) {
          FD_SET(replicas[r_index].fd_outs[p_index], &select_set);
        }

        retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
        for (p_index = 0; p_index < out_pipe_count; p_index++) {
          if (FD_ISSET(replicas[r_index].fd_outs[p_index], &select_set)) {
            // TODO: Need to deal with different pipes being timed?
            replicas[r_index].buff_counts[p_index] = read(replicas[r_index].fd_outs[p_index], replicas[r_index].buffers[p_index], MAX_PIPE_BUFF);

            // If the non-timed pipe outputs multiple times, only the last will be saved.
            if (replicas[r_index].buff_counts[p_index] <= 1) {
              debug_print("Voter - read problem on internal pipe - Controller %s, rep %d, pipe %d\n", controller_name, r_index, p_index);
            }

            if (p_index == timer_stop_index[active_pipe_index]) {
              done_count++;
              done = true;
            }
          } // if FD_ISSET
        } // for pipe
      } // while !done

      // Set rep priority back down
      if (sched_set_policy(replicas[r_index].pid, voter_priority - 4) < 0) {
        // Will fail when the replica is already dead.
        debug_print("sched_set_policy failed: %d, %d\n", replicas[r_index].pid,  voter_priority - 4);
      } else {
        replicas[r_index].priority = voter_priority - 4;
        for_reps[r_index].priority = voter_priority - 4;
      }
    } // if waiting
  } // for replica

  if (!waiting) {
    return -1;
  }

  return done_count;
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

  // I swear that this all makes sense and is a better way to organize everything.
  while(1) {
    // It starts with receiving data. Returns false if nothing received
    if (recvData()) { // sets active_pipe_index (pipe data come in on)
#ifdef SINGLE_CORE
      // The problem here is that sendCollect can easily do the detection itself.
      int reps_done = sendCollect(); // returns -1 if no response expected, # of responses otherwise
      if (-1 == reps_done) {
        // nothing to do, continue on
      } else if (reps_done == rep_count) {
        // check SDCs
        if (checkSDC()) { // checkSDC will have to kill faulty replicas on its own and set fault_index
#ifdef DEBUG_PRINT
          fprintf(stderr, "VoterM(%s) (SC) SDC detected: %d - <%d>\n", controller_name, fault_index, replicas[fault_index].pid);
#else
          fputs("VoterM (SC) SDC detected.\t", stderr);
          fputs(controller_name, stderr);
          fputs("\n", stderr);
#endif
          // SDC found, recover (SMR will never trip SDC)
          startReplicas(false, fault_index, 1); // restarts fault_index
          sendData((fault_index + (rep_count - 1)) % rep_count);
        } else {
          sendData(0); // This is the standard, no faults path
        }
      } else {
        // timeout, fault index already set by sendCollect
#ifdef DEBUG_PRINT
        fprintf(stderr, "VoterM(%s) (SC) CFE or ExecFault detected: %d - <%d>\n", controller_name, fault_index, replicas[fault_index].pid);
#else
        fputs("VoterM (SC) CFE or ExecFault detected.\t", stderr);
        fputs(controller_name, stderr);
        fputs("\n", stderr);
#endif
        killAndClean(fault_index);
        if (1 == rep_count) {
          // With SMR, have to restart the replica from it's exec
          startReplicas(true, 0, 1); // SMR must fork/exec
          sendToReps(); // Ignore return (response is expected)
          collectFromReps(false, 1);
          sendData(0);
        } else {
          // With DMR and TMR fault_index should already be set, and other reps already run.

          // Now set to recover both DMR and TMR cases
          startReplicas(false, fault_index, 1);
          sendData((fault_index + (rep_count - 1)) % rep_count); // TODO: who sends the data?
        }
      }
#endif /* SINGLE_CORE */

#ifdef QUAD_CORE
      // Data received, send to replicas.
      if (sendToReps()) { // returns false if no response is expected. Sends data to replica in pipe[active_pipe_index]
        int reps_done = collectFromReps(true, rep_count); // Set timer and wait for rep_count responses
        // sets timeout_occurred and #done
        if (!timeout_occurred) { // All reps responded in time.
          if (checkSDC()) { // checkSDC will have to kill faulty replicas on its own and set fault_index
#ifdef DEBUG_PRINT
            fprintf(stderr, "VoterM(%s) SDC detected: %d - <%d>\n", controller_name, fault_index, replicas[fault_index].pid);
#else
            fputs("VoterM SDC detected.\t", stderr);
            fputs(controller_name, stderr);
            fputs("\n", stderr);
#endif
            // SDC found, recover (SMR will never trip SDC)
            startReplicas(false, fault_index, 1); // restarts fault_index
            sendData((fault_index + (rep_count - 1)) % rep_count);
          } else {
            sendData(0); // This is the standard, no faults path
          }
        } else { // timeout_occurred
          // Need to make sure killed in case of CFE (for SMR, DMR, and TMR)
          findFaultReplica(); // set fault_index
#ifdef DEBUG_PRINT
          fprintf(stderr, "VoterM(%s) CFE or ExecFault detected: %d - <%d>\n", controller_name, fault_index, replicas[fault_index].pid);
#else
          fputs("VoterM CFE or ExecFault detected.\t", stderr);
          fputs(controller_name, stderr);
          fputs("\n", stderr);
#endif
          killAndClean(fault_index);
          if (1 == rep_count) {
            // With SMR, have to restart the replica from it's exec
            startReplicas(true, 0, 1); // SMR must fork/exec
            sendToReps(); // Ignore return (response is expected)
            collectFromReps(false, 1);
            sendData((fault_index + (rep_count - 1)) % rep_count); // TODO: Always 0?
          } else {
            // With DMR and TMR we need to find the fault rep, and see which healthy reps ran
            // Quad core setups should always have had their healthy reps respond
            // Now set to recover both DMR and TMR cases
            startReplicas(false, fault_index, 1);
            sendData((fault_index + (rep_count - 1)) % rep_count); // TODO: who sends the data?
          }
        } // if / else for timeout_occurred
      } // if(sendToReps) false (no response expected), restart loop
#endif /* QUAD_CORE */

    } // if(recvData) false (no data received) restart loop
  } // while(1)

  return 0;
}
