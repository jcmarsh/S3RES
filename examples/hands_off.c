/*
 * TODO: Desc.
 *
 * March 19, 2014 James Marshall
 */

#include "pint.h"
#include "print_registers.h"
#include <string.h>
#include <sys/wait.h>

#define CHILD_NUM 1
#define BUFF_SIZE 100
#define SEC 0
#define USEC 500000

struct replica_group repGroup;
struct replica replicas[CHILD_NUM];
int insert_error = 0; // Test syscall stuff first
struct timeval tv;

void init() {
  initReplicas(&repGroup, replicas, CHILD_NUM); // Sets up pipes... not needed anymore?
  setupSignal(SIGUSR1);

  // select timeout
  tv.tv_sec = SEC;
  tv.tv_usec = USEC;

  srand(time(NULL));
}

int main(int argc, char** argv) {
  pid_t currentPID = 0;
  int index, status, retval, signal;
  struct user_regs_struct copy_regs;
  
  init();

  //  write_out = launchChildren(&repGroup); // launch a program instead

  // Library-ify this stuff later.

  // Launch the bastards
  for (index = 0; index < CHILD_NUM; index++) {
    currentPID = fork();

    if (currentPID >= 0) { // Successful fork
      if (currentPID == 0) { // Child process
	ptrace(PTRACE_TRACEME, 0, NULL, NULL);
	execl("./fib", "fib", NULL);
      } else { // Parent process
	repGroup.replicas[index].pid = currentPID;
      }
    } else {
      printf("Fork error!\n");
      return -1;
    }
  }

  // Time to snoop on syscalls.
  while(1) {
    // Check for stopped processes
    currentPID = waitpid(-1, &status, 0);
    if (WIFSTOPPED(status)) {
      // In Signal-Delivery-Stop
      signal = WSTOPSIG(status);
      
      switch (signal) {
      case SIGUSR1:
	// Signal we sent (to insert an error)
	break;
      case SIGTRAP:
	// How syscalls are reported
	if (ptrace(PTRACE_GETREGS, currentPID, NULL, &copy_regs) < 0) {
	  perror("GETREGS error.");
	}
	
	// syscall open
	//	if (copy_regs.rax == 5) {
	  printf("SYSCALL PID: %d\n\t", currentPID);
	  PRINT_REG(rax, (&copy_regs)); // eax
	  printf("\t");
	  PRINT_REG(rdi, (&copy_regs)); // ebx
	  printf("\t");
	  PRINT_REG(rsi, (&copy_regs)); // ecx
	  printf("\t");
	  PRINT_REG(rdx, (&copy_regs)); // edx
	  //printRegs(&copy_regs);
	  printf("\n");
	  //	}
	break;
      }
    }
    ptrace(PTRACE_SYSCALL, currentPID, NULL, NULL);
  }
}
