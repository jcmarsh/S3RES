/*
 * fd_client.h
 *
 * James Marshall May 3 2014
 */
#include <fcntl.h>
#include <linux/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/user.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int connectRecvFDS(pid_t pid, int *read_in, int *write_out, const char* name);
