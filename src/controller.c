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
  InitTAS(DEFAULT_CPU, &cpu_speed, priority);

  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    perror("Failed to register the restart handler");
    return -1;
  }

  if (signal(SIGUSR2, testSDCHandler) == SIG_ERR) {
    perror("Failed to register the SDC handler");
    return -1;
  }

  return 0;
}

void restartHandler(int signo) {
  // fork
  pid_t currentPID = fork();
  
  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // child sets new id, recreates connects, loops
      initReplica();
      // Get own pid, send to voter
      currentPID = getpid();
      connectRecvFDS(currentPID, pipes, pipe_count, "ArtPot");
      setPipeIndexes();

      // unblock the signal (restart handler)
      sigset_t signal_set;
      sigemptyset(&signal_set);
      sigaddset(&signal_set, SIGUSR1);
      sigaddset(&signal_set, SIGUSR2);
      sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

      enterLoop(); // return to normal
    } else {   // Parent just returns
      return;
    }
  } else {
    perror("ArtPot Fork error\n");
    return;
  }
}