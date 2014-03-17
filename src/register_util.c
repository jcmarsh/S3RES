#include "register_util.h"
#include "print_registers.h"

void injectRegError(pid_t pid) //struct user_regs_struct * regs)
{
  struct user_regs_struct copy_regs;
  // Create a new bitmask with 1 bit set true... xor 
  // Ought to be a long, no?
  unsigned long error_mask = 1; // Shift right random number of times (between 0 and WORDSIZE)
  int byte_num = 0;
  int reg_num = 0;
  int reg_pick = 0;
  int bit_pick = 0;

  if (ptrace(PTRACE_GETREGS, pid, NULL, &copy_regs) < 0) {
    perror("GETREGS error.");
  }

  byte_num =  __WORDSIZE / 8;
  reg_num =  sizeof(struct user_regs_struct) / byte_num;

  //  printf("byte_num: %d\treg_num: %d\n", byte_num, reg_num);

  // This does not give a unifrom distribution... close enough?
  // Check out http://en.wikipedia.org/wiki/Mersenne_twister
  reg_pick = rand() % reg_num;
  bit_pick = rand() % __WORDSIZE;
  
  printf("reg_pick: %d\tbit_pick: %d\n", reg_pick, bit_pick);

  //  printf("hmmm: %lu\n", *((unsigned long *)regs + reg_pick));
  printf("Old value: %lX\tNew value: %lX\n", *((unsigned long *)&copy_regs + reg_pick), *((unsigned long *)&copy_regs + reg_pick) ^ (error_mask << bit_pick));
  
  *((unsigned long *)&copy_regs + reg_pick) = *((unsigned long *)&copy_regs + reg_pick) ^ (error_mask << bit_pick);

  //  printf("Old value: %lX\tNew value: %lX\n", *((unsigned long *)regs + reg_pick), *((unsigned long *)regs + reg_pick) ^ (error_mask << bit_pick));
  //  *((unsigned long *)regs + reg_pick) = *((unsigned long *)regs + reg_pick) ^ (error_mask << bit_pick);

  if(ptrace(PTRACE_SETREGS, pid, NULL, &copy_regs) < 0) {
    perror("SETREGS error:");
  }
}
