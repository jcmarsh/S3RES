/*
 * fd_client.h
 *
 * James Marshall May 3 2014
 */

#include "commtypes.h"

#include <fcntl.h>
#include <linux/un.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int connectRecvFDS(pid_t pid, struct typed_pipe* pipes, int pipe_count, const char* name, int* pinned_cpu);
