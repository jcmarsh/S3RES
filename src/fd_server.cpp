/*
 * fd_server.cpp
 *
 * James Marshall May 3 2014
 */

#include "../include/fd_server.h"

int sendFDS(int connection_fd, int read_in, int write_out) { // fd names from client perspective
  struct msghdr hdr;
  struct iovec data;
  char cmsgbuf[CMSG_SPACE(sizeof(int) * 2)];

  char dummy = '*';
  data.iov_base = &dummy;
  data.iov_len = sizeof(dummy);

  memset(&hdr, 0, sizeof(hdr));
  hdr.msg_name = NULL;
  hdr.msg_namelen = 0;
  hdr.msg_iov = &data;
  hdr.msg_iovlen = 1;
  hdr.msg_flags = 0;

  hdr.msg_control = cmsgbuf;
  hdr.msg_controllen = CMSG_LEN(sizeof(int) * 2);

  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&hdr);
  cmsg->cmsg_len   = CMSG_LEN(sizeof(int) * 2);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type  = SCM_RIGHTS;

  ((int*)CMSG_DATA(cmsg))[0] = read_in;
  ((int*)CMSG_DATA(cmsg))[1] = write_out;

  int n = sendmsg(connection_fd, &hdr, 0);

  if(n == -1) {
    perror("sendmsg() failed");
  }

  return n;
}

// Name is ./<name>_fd_server
int createFDS(struct server_data * sd, const char* name) {
  // create domain socked
  const char* pre_name = "./";
  const char* post_name = "_fd_server";
  char* actual_name;
  
  asprintf(&actual_name, "%s%s%s", pre_name, name, post_name);

  sd->sock_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (sd->sock_fd < 0) {
    perror("Socket creation failed.");
    return 1;
  }

  printf("Making fd_server: %s\n", actual_name);

  // should delete previous remnants
  unlink(actual_name);

  /* start with a clean address structure */
  memset(&(sd->address), 0, sizeof(struct sockaddr_un));

  sd->address.sun_family = AF_UNIX;
  snprintf(sd->address.sun_path, UNIX_PATH_MAX, actual_name);

  if(bind(sd->sock_fd, (struct sockaddr *) &(sd->address), sizeof(struct sockaddr_un)) != 0) {
    printf("bind() failed\n");
    return 1;
  }

  // This program will listen to connections on this socket
  // 5 is the backlog: how many pending connections there may be
  if(listen(sd->sock_fd, 5) != 0) {
    printf("listen() failed\n");
    return 1;
  }
}

// Blocks on accept: You better know a client is about to connect!
int acceptSendFDS(struct server_data * sd, pid_t *pid, int read_in, int write_out) {
  int connection_fd;
  int retval;

  if ((connection_fd = accept(sd->sock_fd, (struct sockaddr *) &(sd->address), &(sd->address_length))) > -1) {
    sendFDS(connection_fd, read_in, write_out); // send read end to client
  } else {
    perror("FD_Server failed to accept");
    return -1;
  }

  // read pid
  retval = read(connection_fd, pid, sizeof(pid_t));
  if (retval != sizeof(pid_t)) {
    perror("FD_Server failed to read pid");
  }

  return 0;
}
