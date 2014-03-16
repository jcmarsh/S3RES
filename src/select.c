#include "utility.h"

#define CHILD_NUM 3
#define BUFF_SIZE 100
#define SEC 0
#define USEC 500000

typedef enum {
  RUNNING,
  CRASHED,
  FINISHED
} replica_status;

 // Represents one redundant execution, implemented as a thread
struct replica {
  pid_t pid; // The pid of the thread
  int priority; // Not yet implemented
  int pipefd[2]; // pipe to communicate with controller
  // Possibly put a pointer to entry function
  replica_status status;
  unsigned long last_result;
};  

const int starting_n = 42;

// Modify the register structure to have one (uniformily distributed) bit flip.
void injectRegError(struct user_regs_struct * regs) {
  // Create a new bitmask with 1 bit set true... xor 
  // Ought to be a long, no?
  unsigned long error_mask = 1; // Shift right random number of times (between 0 and WORDSIZE)
  int byte_num = 0;
  int reg_num = 0;
  int reg_pick = 0;
  int bit_pick = 0;

  byte_num =  __WORDSIZE / 8;
  reg_num =  sizeof(struct user_regs_struct) / byte_num;

  //  printf("byte_num: %d\treg_num: %d\n", byte_num, reg_num);

  // This does not give a unifrom distribution... close enough?
  // Check out http://en.wikipedia.org/wiki/Mersenne_twister
  reg_pick = rand() % reg_num;
  bit_pick = rand() % __WORDSIZE;
  
  printf("reg_pick: %d\tbit_pick: %d\n", reg_pick, bit_pick);

  //  printf("hmmm: %lu\n", *((unsigned long *)regs + reg_pick));
  printf("Old value: %lX\tNew value: %lX\n", *((unsigned long *)regs + reg_pick), *((unsigned long *)regs + reg_pick) ^ (error_mask << bit_pick));
  *((unsigned long *)regs + reg_pick) = *((unsigned long *)regs + reg_pick) ^ (error_mask << bit_pick);
}

// Find the nth fibonacci number
unsigned long fib(int n) {
  if (n == 1 || n == 2) {
    return 1;
  } else {
    return fib(n-1) + fib(n-2);
  }
}


// Global state
int isChild = 0;
int write_out;
struct replica replicas[CHILD_NUM];
int nfds = 0; // I hate that this is global.
fd_set read_fds;
int insert_error = 1;

// Consider adding a buffer for each replica

// Inititalization
int initReplicas(struct replica* replicas, int num) {
  pid_t currentPID = 0;
  int index = 0;
  int flags = 0;

  // Init three replicas
  for (index = 0; index < num; index++) {
    //    printf("Initing index: %d\n", index);
    if (pipe(replicas[index].pipefd) == -1) {
      printf("Pipe error!\n");
      return 1;
    }

    // Need to set to be non-blocking for reading.
    flags = fcntl(replicas[index].pipefd[0], F_GETFL, 0);
    fcntl(replicas[index].pipefd[0], F_SETFL, flags | O_NONBLOCK);

    // nfds should be the highes file descriptor, plus 1
    // TODO: This may have to be changed for when signal fd is added
    if (replicas[index].pipefd[0] >= nfds) {
      nfds = replicas[index].pipefd[0] + 1;
    }
    // Set to select on pipe's file descriptor
    FD_SET(replicas[index].pipefd[0], &read_fds);

    replicas[index].pid = -1;
    replicas[index].priority = -1;
    replicas[index].last_result = 0;
    replicas[index].status = RUNNING;
  }

  // Fork children
  for (index = 0; index < CHILD_NUM; index++) {
    currentPID = fork();
    
    if (currentPID >= 0) { // Successful fork
      if (currentPID == 0) { // Child process
	isChild = 1;
	write_out = replicas[index].pipefd[1];
	break;
      } else { // Parent Process
	replicas[index].pid = currentPID;
      }
    } else {
      printf("Fork error!\n");
      return 1;
    }
  }

  // TODO: Errors
  return 0;
}

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

void replicaCrash(pid) {
  int index;

  kill(pid, SIGKILL);
  for (index = 0; index < CHILD_NUM; index++) {
    if (replicas[index].pid == pid) {
      replicas[index].status = CRASHED;
    }
  }
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

int main(int argc, char** argv) {
  pid_t currentPID = 0;
  int index = 0;
  int status = -1;
  char buffer[BUFF_SIZE] = {0};

  // select stuff
  struct timeval tv;
  int retval;

  int countdown = 20;
  int still_running = 0;

  setupSignal(SIGUSR1);

  // select timeout
  FD_ZERO(&read_fds);
  tv.tv_sec = SEC;
  tv.tv_usec = USEC;

  //
  srand(time(NULL));

  // Init replicas and fork children
  initReplicas(replicas, CHILD_NUM);

  if (isChild) {
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
	handleProcess(currentPID, status);
      }

      // Select over pipes from replicas
      retval = select(nfds, &read_fds, NULL, NULL, &tv);

      tv.tv_sec = SEC;
      tv.tv_usec = USEC;


      if (retval == -1) {
	perror("select()");
      } else {
	// Data to read! Loop through and print
	for (index = 0; index < CHILD_NUM; index++) {
	  retval = read(replicas[index].pipefd[0], buffer, BUFF_SIZE);
	  if (retval > 0) {
	    replicas[index].last_result = atol(buffer);
	    replicas[index].status = FINISHED;
	    memset(buffer, 0, BUFF_SIZE);
	  }
	}
      }
    }

    // All done!
    printResults(replicas, CHILD_NUM);
  }

  return 0;
}
