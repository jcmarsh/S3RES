/*
 * Operations dealing with system registers.
 * Should be able to handle x86 and x86_64 for now.
 *
 * March 17th, 2014 James Marshall
 */

#include <sys/user.h>
#include <sys/ptrace.h>

// For the printfs...
#include <stdio.h>
#include <stdlib.h>

#include "print_registers.h"

// Modify the register structure to have one (not quite? uniformily distributed) bit flip.
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
    return;
  }

  byte_num =  __WORDSIZE / 8;
  reg_num =  sizeof(struct user_regs_struct) / byte_num;

  //  printf("byte_num: %d\treg_num: %d\n", byte_num, reg_num);

  // This does not give a unifrom distribution... close enough?
  // Check out http://en.wikipedia.org/wiki/Mersenne_twister

  /* FOR x86_64
   * Segment registers are only 16 bits.
   * 4 of the 6 segment register can not be changed (ds, es, fs, and gs)
   * fs_base and gs_base are 47 bits
   * eflags is 18 bits. - Errors when larger bit set... so keeping
   * Can't seem to set bits 0 and 1 of cs or ss reg
   */

  reg_pick = rand() % (reg_num -4); // For now skip the last 4 regs in x86_64
  if (17 == reg_pick || 20 == reg_pick) {
    bit_pick = (rand() % (__WORDSIZE - 2)) + 2; // No 0 or 1 for cs and ss
  } else if (21 == reg_pick || 22 == reg_pick) { // fs_base and gs_base are 47 bits
    bit_pick = rand() % 47;
  } else {
    bit_pick = rand() % __WORDSIZE;
  }

  // printf("reg_pick: %d\tbit_pick: %d\n", reg_pick, bit_pick);

  //  printf("hmmm: %lu\n", *((unsigned long *)regs + reg_pick));
  //printf("Old value: %lX\tNew value: %lX\n", *((unsigned long *)&copy_regs + reg_pick), *((unsigned long *)&copy_regs + reg_pick) ^ (error_mask << bit_pick));
  
  *((unsigned long *)&copy_regs + reg_pick) = *((unsigned long *)&copy_regs + reg_pick) ^ (error_mask << bit_pick);

  //  printf("Old value: %lX\tNew value: %lX\n", *((unsigned long *)regs + reg_pick), *((unsigned long *)regs + reg_pick) ^ (error_mask << bit_pick));
  //  *((unsigned long *)regs + reg_pick) = *((unsigned long *)regs + reg_pick) ^ (error_mask << bit_pick);

  if(ptrace(PTRACE_SETREGS, pid, NULL, &copy_regs) < 0) {
    printf("SETREGS error for reg %d, bit %d\n", reg_pick, bit_pick);
    perror("SETREGS error:");
  }
}

void main(int argc, char** argv) {
  // Should be simple: inject a bit flip into the process specified by argument
  pid_t attack_pid = 0;
  time_t t;
  srand((unsigned) time(&t));
	
  if (argc < 2) {
    printf("Usage: inject_error <pid>\n");
    return;
  } else {
    attack_pid = atoi(argv[1]);
  }
  
  // Attach stops the process
  if (ptrace(PTRACE_ATTACH, attack_pid, NULL, NULL) < 0) {
    perror("Failed to attach");
  }
  waitpid(attack_pid);
  
  injectRegError(attack_pid);
  
  if (ptrace(PTRACE_CONT, attack_pid, NULL, NULL) < 0) {
    perror("Failed to resume");
  }
}
