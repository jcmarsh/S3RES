#include "../include/controller.h"

extern void setPipeIndexes(void);
extern void enterLoop(void);
extern void testSDCHandler(int signo, siginfo_t *si, void *unused);

extern int priority;
extern int pipe_count;
extern struct typed_pipe pipes[];
extern const char* name;

int initController(void) {
  struct sigaction sa;

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = restartHandler;
  if (sigaction(RESTART_SIGNAL, &sa, NULL) == -1) {
    perror("Failed to register the restart handler");
    return -1;
  }

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = testSDCHandler;
  if (sigaction(SDC_SIM_SIGNAL, &sa, NULL) == -1) {
    perror("Failed to register the simulate sdc handler");
    return -1;
  }

  InitTAS(DEFAULT_CPU, priority);
  
  return 0;
}

static void restartHandler(int signo, siginfo_t *si, void *unused) {
  #ifdef TIME_RESTART_SIGNAL
    timestamp_t curr_time = generate_timestamp();
    timestamp_t parent_time = (timestamp_t)si->si_value.sival_ptr;
    printf("(%ld)\n", curr_time - parent_time);
  #endif
    
  int index = 0;

  // fork
  pid_t currentPID = fork();
  
  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // child sets new id, recreates connects, loops
      initController();

      // clean up pipes
      for (index = 0; index < pipe_count; index++) {
        resetPipe(&pipes[index]);
      }

      // unblock the signals (restart handler, inject SDC)
      sigset_t signal_set;
      sigemptyset(&signal_set);
      sigaddset(&signal_set, RESTART_SIGNAL);
      sigaddset(&signal_set, SDC_SIM_SIGNAL);
      if (sigprocmask(SIG_UNBLOCK, &signal_set, NULL) < 0) {
        perror("Controller signal unblock error");
      }

      EveryTAS();

      // Get own pid, send to voter
      currentPID = getpid();
      if (connectRecvFDS(currentPID, pipes, pipe_count, name) < 0) {
        printf("Error in %s: failed connectRecvFDS call.\n", name);
        return;
      }
      
      setPipeIndexes();
      
      return;
    } else {   // Parent just returns
      return;
    }
  } else {
    printf("%s ", name); 
    perror("Fork error\n");
    return;
  }
}
