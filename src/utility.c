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

int handleProcess(struct replica_group *rg, pid_t pid, int status, int insert_error) {
  int signal = -1;
  int error_inserted = 0;

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
	// Inject an error: for now a bit flip in a register
	injectRegError(pid);
	error_inserted = 1;
      }
      ptrace(PTRACE_CONT, pid, NULL, NULL);
      break;
    case SIGCONT:
      // Should likely do something with this signal, no? Pass on at least?
      printf("Caught a continue.\n");
      break;
    case SIGILL:
      // Illegal Instruction: Kill process. #4
      replicaCrash(rg, pid);
      break;
    case SIGBUS:
      // Bus error (bad memory access): Kill process. #7
      replicaCrash(rg, pid);
      break;
    case SIGSEGV:
      // Invalid memory reference: Kill process. #11
      replicaCrash(rg, pid);
      break;
    case SIGCHLD:
      // Child Stoped or terminated: ignore. #17

    default:
      printf("Unhandled signal: %d\n", signal);
      break;
    }

    // Not sure about this. 
    // Do not suppress signal, pass on.
    ptrace(PTRACE_CONT, pid, 0, signal);
  }
  // TODO: return? something special if error inserted? Error?
  return error_inserted;
}

// Can not seem to have an arbitrary length for the second array dimension
void printResultsDoubles(struct replica* replicas, int child_num, double results[][2]) { //, int arg_num) {
  int index, andex;
  int arg_num = 2;
  char outcome = 'B';
  double prev_result[arg_num];

  for (andex = 0; andex < arg_num; andex++) {
    prev_result[andex] = results[0][andex];
  }

  // Compare all results to find data corruption
  for (index = 0; index < child_num; index++) {
    printf("\tRestuls %d:", index);
    for (andex = 0; andex < arg_num; andex++) {
      printf("\t%f", results[index][andex]);
      if (prev_result[andex] != results[index][andex]) {
	outcome = 'S';
      }
      prev_result[andex] = results[index][andex];
    }
    printf("\n");
  }

  // TODO: Can likely split out. Would be good to separate replica status from output handling
  // Check for Crashes and Timeouts
  for (index = 0; index < child_num; index++) {
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

/*
 * Doesn't seem to be the right way to go about it,
 * but I'm not sure how I want to deal with yet.
 */
void printResultsULong(struct replica* replicas, int child_num, unsigned long* results) {
  int index;
  unsigned long prev_result = results[0];
  char outcome = 'B'; // B for Benign

  for (index = 0; index < child_num; index++) {
    printf("\tResult %d: %lu\n", index, results[index]);
    if (prev_result != results[index]) {
      outcome = 'S'; // S for SILENT DATA CORRUPTION!
    }
    prev_result = results[index];
  }

  // Check for Crashes and Timeouts
  for (index = 0; index < child_num; index++) {
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
