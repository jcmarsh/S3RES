#include "../include/controller.h"

extern void setPipeIndexes(void);
extern void enterLoop(void);
extern void testSDCHandler(int signo);

extern cpu_speed_t cpu_speed;
extern int priority;
extern int pipe_count;
extern struct typed_pipe pipes[];
extern const char* name;

int initReplica(void) {
  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    perror("Failed to register the restart handler");
    return -1;
  }

  if (signal(SIGUSR2, testSDCHandler) == SIG_ERR) {
    perror("Failed to register the SDC handler");
    return -1;
  }
  InitTAS(DEFAULT_CPU, &cpu_speed, priority);
  
  return 0;
}

void restartHandler(int signo) {
  // fork
  pid_t currentPID = fork();
  
  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // child sets new id, recreates connects, loops
      initReplica();

      // clean up pipes
      for (int i = 0; i < pipe_count; i++) {
        resetPipe(&pipes[i]);
      }

      // Get own pid, send to voter
      currentPID = getpid();
      if (connectRecvFDS(currentPID, pipes, pipe_count, name) < 0) {
        printf("Error in %s: failed connectRecvFDS call.\n", name);
        return;
      }
      setPipeIndexes();

      // unblock the signal (restart handler - USR1, inject SDC - USR2)
      sigset_t signal_set;
      sigemptyset(&signal_set);
      sigaddset(&signal_set, SIGUSR1);
      sigaddset(&signal_set, SIGUSR2);
      sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

      EveryTAS();

      enterLoop(); // return to normal
    } else {   // Parent just returns
      return;
    }
  } else {
    printf("%s ", name); 
    perror("Fork error\n");
    return;
  }
}
