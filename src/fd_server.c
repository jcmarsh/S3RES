/*
 * fd_server.c
 *
 * James Marshall May 3 2014
 */

#include "../include/fd_server.h"

int sendFDS(int connection_fd, struct vote_pipe* pipes, int pipe_count, int pinned_cpu) { // pipes are the rep side
  int i;
  struct msghdr hdr;
  struct iovec datas[2];
  // cmsg is the out-of-band data (fds)
  char cmsgbuf[CMSG_SPACE(sizeof(int) * pipe_count)];

  struct typed_pipe send_pipes[pipe_count];
  convertVoteToTyped(pipes, pipe_count, send_pipes);

  datas[0].iov_base = send_pipes;
  datas[0].iov_len = sizeof(send_pipes);

  datas[1].iov_base = &pinned_cpu;
  datas[1].iov_len = sizeof(pinned_cpu);

  memset(&hdr, 0, sizeof(hdr));
  hdr.msg_name = NULL;
  hdr.msg_namelen = 0;
  hdr.msg_iov = datas;
  hdr.msg_iovlen =  2; // number of iovec items in datas
  hdr.msg_flags = 0;

  hdr.msg_control = cmsgbuf;
  hdr.msg_controllen = CMSG_LEN(sizeof(int) * pipe_count);

  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&hdr);
  cmsg->cmsg_len   = CMSG_LEN(sizeof(int) * pipe_count);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type  = SCM_RIGHTS;

  for (i = 0; i < pipe_count; i++) {
    if (pipes[i].fd_in != 0) {
      ((int*)CMSG_DATA(cmsg))[i] = pipes[i].fd_in;
    } else {
      ((int*)CMSG_DATA(cmsg))[i] = pipes[i].fd_out;
    }
  }

  int retval = TEMP_FAILURE_RETRY(sendmsg(connection_fd, &hdr, 0));

  if(retval < 0) {
    perror("FD_server sendmsg() failed");
  }

  return retval;
}

// Name is ./<name>_fd_server
int createFDS(struct server_data * sd, const char* name) {
  // create domain socked
  const char* pre_name = "./";
  const char* post_name = "_fd_server";
  char* actual_name;
  
  if (asprintf(&actual_name, "%s%s%s", pre_name, name, post_name) < 0) {
    perror("fd_server failed to allocate fd_name");
  }

  sd->sock_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (sd->sock_fd < 0) {
    perror("Socket creation failed.");
    return 1;
  }

  // should delete previous remnants
  unlink(actual_name);

  /* start with a clean address structure */
  memset(&(sd->address), 0, sizeof(struct sockaddr_un));

  sd->address.sun_family = AF_UNIX;
  if (strlen(actual_name) < UNIX_PATH_MAX) {
    memcpy(&(sd->address.sun_path), actual_name, strlen(actual_name));
  } else {
    printf("Server Address length longer than max.\n");
    return -1;
  }
  free(actual_name);

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

/*
 * 
 * Blocks on accept: You better know a client is about to connect!
 * Returns 0 upon success, <0 otherwise.
 */
int acceptSendFDS(struct server_data * sd, pid_t *pid, struct vote_pipe* pipes, int pipe_count, int pinned_cpu) {
  int connection_fd;
  int retval = 0;

  connection_fd = TEMP_FAILURE_RETRY(accept(sd->sock_fd, (struct sockaddr *) &(sd->address), &(sd->address_length)));
  if (connection_fd > -1) {
    // send read end to client
    if (sendFDS(connection_fd, pipes, pipe_count, pinned_cpu) < 0) {
      perror("FD_Server failed to sendFDS");
      retval = -1;
      goto accept_send_FDS_out;
    }
  } else {
    perror("FD_Server failed to accept");
    retval = -1;
    goto accept_send_FDS_out;
  }

  // read pid
  retval = TEMP_FAILURE_RETRY(read(connection_fd, pid, sizeof(pid_t)));
  if (retval != sizeof(pid_t)) {
    perror("FD_Server failed to read pid");
    retval = -1;
  }

accept_send_FDS_out:
  close(connection_fd);
  return retval;
}
