/*
 * fd_client.h
 *
 * James Marshall May 3 2014
 */

#include "commtypes.h"

#include <fcntl.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <stdlib.h>

int connectRecvFDS(pid_t pid, struct typed_pipe* pipes, int pipe_count, const char* name, int* pinned_cpu);
