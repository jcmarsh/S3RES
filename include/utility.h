/*
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/wait.h>
*/

int setupSignal(int signal_ignored);

int handleProcess(pid_t pid, int status);

void printResults(struct replica* replicas, int num);
