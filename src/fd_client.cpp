/*
 * fd_client.cpp
 *
 * James Marshall May 3 2014
 */

#include "../include/fd_client.h"

int requestFDS(int sock, int * read_in, int * write_out) {
  struct msghdr hdr;
  struct iovec data;
  struct cmsghdr *cmsg;
  int retval;
  
  char dummy = '*';

  data.iov_base = &dummy;
  data.iov_len = sizeof(dummy);

  memset(&hdr, 0, sizeof(hdr));
  hdr.msg_name = NULL;
  hdr.msg_namelen = 0;
  hdr.msg_iov = &data;
  hdr.msg_iovlen = 1;
  hdr.msg_flags = 0;

  cmsg = (cmsghdr*)malloc(CMSG_LEN(sizeof(int) * 2));
  hdr.msg_control = cmsg;
  hdr.msg_controllen = CMSG_LEN(sizeof(int) * 2);

  retval = recvmsg(sock, &hdr, 0);
  if (retval < 0) {
    perror("FD_client RECVMSG error");
    return -1;
  }

  *read_in = ((int *)CMSG_DATA(cmsg))[0];
  *write_out = ((int *)CMSG_DATA(cmsg))[1];

  free(cmsg);
  return 0;
}

int connectRecvFDS(pid_t pid, int *read_in, int *write_out) {
  int sock_fd;
  int retval;
  struct sockaddr_un address;
  struct msghdr hdr;

  sock_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if(sock_fd < 0) {
    perror("Replica socket() failed");
    return -1;
  }

  /* start with a clean address structure */
  memset(&address, 0, sizeof(struct sockaddr_un));
 
  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, UNIX_PATH_MAX, "./fd_server");

  if(connect(sock_fd, (struct sockaddr *) &address, sizeof(struct sockaddr_un)) != 0) {
    perror("Replica connect() failed");
    return -1;
  }

  retval = requestFDS(sock_fd, read_in, write_out);
  // TODO: check retval

  // Send pid
  write(sock_fd, &pid, sizeof(pid_t));

  close(sock_fd);

  return 0;
}
