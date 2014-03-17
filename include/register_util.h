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

// Modify the register structure to have one (uniformily distributed) bit flip.
/*
 * returns 1 if an error is 
 */
void injectRegError(pid_t pid);
