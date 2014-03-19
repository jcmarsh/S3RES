/*
 * Should actually test the libraries I am making.
 */

#include "pint.h"
#include <string.h>
#include <sys/wait.h>

#define CHILD_NUM 3
#define BUFF_SIZE 100
#define SEC 0
#define USEC 500000

const int starting_n = 42;


// Find the nth fibonacci number
unsigned long fib(int n) {
  if (n == 1 || n == 2) {
    return 1;
  } else {
    return fib(n-1) + fib(n-2);
  }
}

// Global Data
int write_out;
struct replica_group repGroup;
struct replica replicas[CHILD_NUM];
int insert_error = 1;
struct timeval tv;
 
// Inititalization
void init() {
  initReplicas(&repGroup, replicas, CHILD_NUM);
  setupSignal(SIGUSR1);


  // select timeout
  tv.tv_sec = SEC;
  tv.tv_usec = USEC;

  srand(time(NULL));
}

int main(int argc, char** argv) {
  pid_t currentPID = 0;
  int index = 0;
  int status = -1;
  char buffer[BUFF_SIZE] = {0};
  int retval;
  int countdown = 20;
  int still_running = 0;
  unsigned long results[CHILD_NUM];

  init();

  write_out = launchChildren(&repGroup);

 if (write_out != 0) {
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);

    snprintf(buffer, BUFF_SIZE, "%lu", fib(starting_n));
    write(write_out, buffer, BUFF_SIZE);
  } else { // Main control / voting loop
    while(1) {
      // check if any replicas are still running
      still_running = 0;
      for (index = 0; index < CHILD_NUM; index++) {
	if (replicas[index].status == RUNNING) {
	  still_running++;
	}
      }
      if (still_running == 0) {
	// All replicas are finished (or crashed)
	break;
      }

      countdown--;
      if (countdown < 0) {
	// Time's up
	break;
      }

      // Insert an error?
      // Always insert an error for the third process
      if (insert_error) {
	kill(replicas[2].pid, SIGUSR1);
	//insert_error = 0;
      }

      // Check for stopped processes
      currentPID = waitpid(-1, &status, WNOHANG);
      if (currentPID == 0) {
	// no pending process
      } else {
	// Handle pending process
	if (handleProcess(&repGroup, currentPID, status, insert_error) == 1) {
	  insert_error = 0;
	}
      }

      // Select over pipes from replicas
      retval = select(repGroup.nfds, &(repGroup.read_fds), NULL, NULL, &tv);

      tv.tv_sec = SEC;
      tv.tv_usec = USEC;


      if (retval == -1) {
	perror("select()");
      } else {
	// Data to read! Loop through and print
	for (index = 0; index < CHILD_NUM; index++) {
	  retval = read(replicas[index].pipefd[0], buffer, BUFF_SIZE);
	  if (retval > 0) {
	    results[index] = atol(buffer);
	    replicas[index].status = FINISHED;
	    memset(buffer, 0, BUFF_SIZE);
	  }
	}
      }
    }

    // All done!
    printResults(replicas, results, CHILD_NUM);
  }

  return 0;
}

