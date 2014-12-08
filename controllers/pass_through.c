/*
 * Simple filter that averages the three values previous values for
 * Ranger readings.
 *
 * James Marshall
 */

#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../include/taslimited.h"
#include "../include/commtypes.h"
#include "../include/fd_client.h"


void enterLoop();
void command();
int initReplica();

void restartHandler(int signo) {
  // fork
  timestamp_t last = generate_timestamp();
  pid_t currentPID = fork();


  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      timestamp_t current = generate_timestamp();
      printf("%lld\n", current - last);

      if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
        perror("Failed to register the restart handler");
      }

      // unblock the signal
      sigset_t signal_set;
      sigemptyset(&signal_set);
      sigaddset(&signal_set, SIGUSR1);
      sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

      return;
    } else {   // Parent just returns
      waitpid(-1, NULL, WNOHANG);
      return;
    }
  } else {
    printf("Fork error!\n");
    return;
  }
}

int main(int argc, const char **argv) {
  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    perror("Failed to register the restart handler");
    return -1;
  }

  while(1) {
    sleep(1);
  }

  return 0;
}
