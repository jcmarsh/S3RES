/*
 * fd_client.cpp
 *
 * James Marshall May 3 2014
 */

#include "../include/fd_client.h"
#include "../include/commtypes.h"

int requestFDS(int sock_fd, struct typed_pipe* pipes, int pipe_count) {
  struct msghdr hdr;
  struct iovec data;
  struct cmsghdr *cmsg;
  int retval;
  
  int types_msg[pipe_count * 2];
  data.iov_base = types_msg;
  data.iov_len = sizeof(types_msg);

  memset(&hdr, 0, sizeof(hdr));
  hdr.msg_name = NULL;
  hdr.msg_namelen = 0;
  hdr.msg_iov = &data;
  hdr.msg_iovlen = 1; // number of iovec data structs, here only one
  hdr.msg_flags = 0;

  cmsg = (cmsghdr*)malloc(CMSG_LEN(sizeof(int) * pipe_count));
  hdr.msg_control = cmsg;
  hdr.msg_controllen = CMSG_LEN(sizeof(int) * pipe_count);

  retval = recvmsg(sock_fd, &hdr, 0);
  if (retval < 0) {
    perror("FD_client: RECVMSG error");
    return -1;
  }

  // recover types_msg
  for (int i = 0; i < pipe_count * 2; i = i + 2) {
    pipes[i/2].type = (comm_message_t) types_msg[i];
    if (types_msg[i + 1] == 0) { // the pipe is a read side
      pipes[i/2].fd_in = 1; // Set to actual fd in a hot minute
      pipes[i/2].fd_out = 0;
    } else {
      pipes[i/2].fd_in = 0;
      pipes[i/2].fd_out = 1;
    }
  }

  // recover pipe fds
  for (int i = 0; i < pipe_count; i++) {
    if (pipes[i].fd_in != 0) {
      pipes[i].fd_in = ((int *)CMSG_DATA(cmsg))[i];
    } else {
      pipes[i].fd_out = ((int *)CMSG_DATA(cmsg))[i];
    }
  }

  free(cmsg);
  return retval;
}

int connectRecvFDS(pid_t pid, struct typed_pipe* pipes, int pipe_count, const char* name) {
  int sock_fd;
  int retval = 0;
  struct sockaddr_un address;
  const char* pre_name = "./";
  const char* post_name = "_fd_server";
  char* actual_name;

  if (asprintf(&actual_name, "%s%s%s", pre_name, name, post_name) < 0) {
    perror("fd_client failed to allocate fd name");
    retval = -1;
    goto connect_recv_FDS_out;
  }

  sock_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if(sock_fd < 0) {
    perror("Replica socket() failed");
    retval = -1;
    goto connect_recv_FDS_out;
  }

  /* start with a clean address structure */
  memset(&address, 0, sizeof(struct sockaddr_un));
 
  address.sun_family = AF_UNIX;
  if (strlen(actual_name) < UNIX_PATH_MAX) {
    memcpy(&(address.sun_path), actual_name, strlen(actual_name));
  } else {
    printf("Client Address length longer than max.\n");
    retval = -1;
    goto connect_recv_FDS_sock_out;
  }
  //snprintf(address.sun_path, UNIX_PATH_MAX, actual_name);
  
  if(connect(sock_fd, (struct sockaddr *) &address, sizeof(struct sockaddr_un)) != 0) {
    perror("Replica connect() failed");
    retval = -1;
    goto connect_recv_FDS_sock_out;
  }

  if (requestFDS(sock_fd, pipes, pipe_count) < 0) {
    retval = -1;
    goto connect_recv_FDS_sock_out;
  }

  // Send pid
  if (write(sock_fd, &pid, sizeof(pid_t)) < 0) {
    perror("FD_client write for pid failed");
  }

connect_recv_FDS_sock_out:
  close(sock_fd);
connect_recv_FDS_out:
  free(actual_name);
  return retval;
}
