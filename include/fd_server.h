/*
 * fd_server.h
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

struct server_data{
  struct sockaddr_un address;
  socklen_t address_length;
  int sock_fd;
};

int createFDS(struct server_data * sd);

int acceptSendFDS(struct server_data * sd, int read_in, int write_out);
