#include "controller.h"
#include "taslimited.h"

#ifdef TIME_RESTART_SIGNAL
#include "system_config.h"
#endif

// All extern here must be in the controller code (such as art_pot.c)
extern void setPipeIndexes(void);

extern int pipe_count;
extern struct typed_pipe pipes[];
extern const char* name;
extern int pinned_cpu;
extern int priority;

extern bool insertSDC;
extern bool insertCFE;

static void restartHandler(int signo, siginfo_t *si, void *unused);

void testSDCHandler(int signo, siginfo_t *si, void *unused) {
  insertSDC = true;
}

void testCFEHandler(int signo, siginfo_t *si, void *unused) {
  insertCFE = true;
}

// Pipes should already be initialized by parseArgs or connectRecvFDS
int initController(void) {
  struct sigaction sa;

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = restartHandler;
  if (sigaction(RESTART_SIGNAL, &sa, NULL) == -1) {
    debug_print("Failed to register the restart handler.\n");
    return -1;
  }

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = testSDCHandler;
  if (sigaction(SDC_SIM_SIGNAL, &sa, NULL) == -1) {
    debug_print("Failed to register the simulate sdc handler.\n");
    return -1;
  }

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = testCFEHandler;
  if (sigaction(CFE_SIM_SIGNAL, &sa, NULL) == -1) {
    debug_print("Failed to register the simulate sdc handler.\n");
    return -1;
  }

  InitTAS(pinned_cpu, priority);

  debug_print("Initializing controller %s\n", name);
  return 0;
}

// TODO: really need to check the flow of priority here
static void restartHandler(int signo, siginfo_t *si, void *unused) {
  #ifdef TIME_RESTART_SIGNAL
    timestamp_t curr_time = generate_timestamp();
    timestamp_t parent_time = (timestamp_t)si->si_value.sival_ptr;
    printf("Signal Time - usec (%lf)\n", diff_time(curr_time, parent_time, CPU_MHZ));
  #endif

  int index = 0;

  // fork
  pid_t currentPID = fork();
  
  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // child sets new id, recreates connects, loops
      // clean up pipes
      for (index = 0; index < pipe_count; index++) {
        resetPipe(&pipes[index]);
      }

      // unblock the signals (restart handler, inject SDC)
      sigset_t signal_set;
      sigemptyset(&signal_set);
      sigaddset(&signal_set, RESTART_SIGNAL);
      sigaddset(&signal_set, SDC_SIM_SIGNAL);
      sigaddset(&signal_set, CFE_SIM_SIGNAL);
      if (sigprocmask(SIG_UNBLOCK, &signal_set, NULL) < 0) {
        debug_print("Controller signal unblock error.\n");
      }

      // InitTAS before connectRecvFDS forces memory operations before voter continues.
      InitTAS(pinned_cpu, priority);

      // Get own pid, send to voter
      currentPID = getpid();
      if (connectRecvFDS(currentPID, pipes, pipe_count, name, &pinned_cpu, &priority) < 0) {
        debug_print("Error in %s: failed connectRecvFDS call.\n", name);
        return;
      }
      
      setPipeIndexes(); // Needs to be AFTER new pipe fds have been attained in connectRecvFDS. Crazy, huh?
      
      return;
    } else {   // Parent needs to re-lock / walk own pages
      RefreshTAS();
      return;
    }
  } else {
    debug_print("Fork error - %s ", name);
    return;
  }
}
