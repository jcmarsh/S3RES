/*
 * fd_client.h
 *
 * James Marshall May 3 2014
 */

#define _GNU_SOURCE 1

#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/un.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/commtypes.h"

int connectRecvFDS(pid_t pid, struct typed_pipe* pipes, int pipe_count, const char* name);
