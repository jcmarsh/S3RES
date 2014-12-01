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
  int retval;
  struct sockaddr_un address;
  struct msghdr hdr;
  const char* pre_name = "./";
  const char* post_name = "_fd_server";
  char* actual_name;

  retval = asprintf(&actual_name, "%s%s%s", pre_name, name, post_name);

  sock_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if(sock_fd < 0) {
    perror("Replica socket() failed");
    return -1;
  }

  /* start with a clean address structure */
  memset(&address, 0, sizeof(struct sockaddr_un));
 
  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, UNIX_PATH_MAX, actual_name);
  free(actual_name);
  
  if(connect(sock_fd, (struct sockaddr *) &address, sizeof(struct sockaddr_un)) != 0) {
    perror("Replica connect() failed");
    return -1;
  }

  retval = requestFDS(sock_fd, pipes, pipe_count);
  if (retval < 0) {
    close(sock_fd);
    return -1;
  }

  // Send pid
  retval = write(sock_fd, &pid, sizeof(pid_t));
  if (retval < 0) {
    perror("FD_client write for pid failed");
  }

  close(sock_fd);

  return retval;
}
