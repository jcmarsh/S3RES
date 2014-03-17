#include "utility.h"

int setupSignal(int signal_ignored) {
  struct sigaction new_sa;
  struct sigaction old_sa;

  // Set to ignore signal (which will in turn trip ptrace)
  sigfillset(&new_sa.sa_mask);
  new_sa.sa_handler = SIG_IGN;
  new_sa.sa_flags = 0;

  if (sigaction(signal_ignored, &new_sa, &old_sa) == 0) {
    sigaction(SIGINT, &new_sa, 0);
  }

  // TODO: Handle errors
  return 0;
}

int handleProcess(pid_t pid, int status) {
  int signal = -1;
  struct user_regs_struct copy_regs;

  if (WIFEXITED(status)) {

  }
  if (WIFSIGNALED(status)) {

  }
  if (WIFSTOPPED(status)) {
    // In Signal-Delivery-Stop
    signal = WSTOPSIG(status);

    switch (signal) {
    case SIGUSR1:
      // Hopefully was the signal we sent... so insert an error
      if (insert_error == 0) {
	// Do nothing, error already has been inserted
      } else {
	insert_error = 0;
	    
	if (ptrace(PTRACE_GETREGS, pid, NULL, &copy_regs) < 0) {
	  perror("GETREGS error.");
	}

	// Inject an error: for now a bit flip in a register
	injectRegError(&copy_regs);

	if(ptrace(PTRACE_SETREGS, pid, NULL, &copy_regs) < 0) {
	  perror("SETREGS error:");
	}
      }
      ptrace(PTRACE_CONT, pid, NULL, NULL);
      break;
    case SIGCONT:
      // Should likely do something with this signal, no? Pass on at least?
      printf("Caught a continue.\n");
      break;
    case SIGILL:
      // Illegal Instruction: Kill process. #4
      replicaCrash(pid);
      break;
    case SIGBUS:
      // Bus error (bad memory access): Kill process. #7
      replicaCrash(pid);
      break;
    case SIGSEGV:
      // Invalid memory reference: Kill process. #11
      replicaCrash(pid);
      break;
    case SIGCHLD:
      // Child Stoped or terminated: ignore. #17

    default:
      printf("Unhandled signal: %d\n", signal);
      break;
    }
  }
}

void printResults(struct replica* replicas, int num) {
  int index;
  long prev_result = replicas[0].last_result;
  char outcome = 'B'; // B for Benign

  for (index = 0; index < num; index++) {
    printf("\tResult %d: %lu\n", index, replicas[index].last_result);
    if (prev_result != replicas[index].last_result) {
      outcome = 'S'; // S for SILENT DATA CORRUPTION!
    }
    prev_result = replicas[index].last_result;
  }

  // Check for Crashes and Timeouts
  for (index = 0; index < num; index++) {
    switch (replicas[index].status) {
    case RUNNING:
      outcome = 'T';
      break;
    case CRASHED:
      outcome = 'C';
      break;
    }
  }

  printf("RESULT: %c\n", outcome);
}
