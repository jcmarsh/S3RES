/*
 * fd_client.c
 *
 * James Marshall May 3 2014
 */

#include "../include/fd_client.h"
#include "../include/commtypes.h"

int requestFDS(int sock_fd, struct typed_pipe* pipes, int pipe_count, int *pinned_cpu, int *priority) {
  int i;
  struct msghdr hdr;
  struct iovec datas[3];
  struct cmsghdr *cmsg;
  int retval;
  
  datas[0].iov_base = pipes;
  datas[0].iov_len = sizeof(struct typed_pipe) * pipe_count;

  datas[1].iov_base = pinned_cpu;
  datas[1].iov_len = sizeof(int);

  datas[2].iov_base = priority;
  datas[2].iov_len = sizeof(int);

  memset(&hdr, 0, sizeof(hdr));
  hdr.msg_name = NULL;
  hdr.msg_namelen = 0;
  hdr.msg_iov = datas;
  hdr.msg_iovlen = 3; // number of iovec data structs
  hdr.msg_flags = 0;

  cmsg = (struct cmsghdr*)malloc(CMSG_LEN(sizeof(int) * pipe_count));
  hdr.msg_control = cmsg;
  hdr.msg_controllen = CMSG_LEN(sizeof(int) * pipe_count);

  retval = recvmsg(sock_fd, &hdr, 0);
  if (retval < 0) {
    perror("FD_client: RECVMSG error");
    return -1;
  }

  // recover pipe fds
  for (i = 0; i < pipe_count; i++) {
    if (pipes[i].fd_in != 0) {
      pipes[i].fd_in = ((int *)CMSG_DATA(cmsg))[i];
    } else {
      pipes[i].fd_out = ((int *)CMSG_DATA(cmsg))[i];
    }
  }

  free(cmsg);
  return retval;
}

/*
 * Connects to the named file descriptor server, receives new fds for each pipe, and sends back its PID.
 * Returns 0 on success, <0 otherwise.
 */
int connectRecvFDS(pid_t pid, struct typed_pipe* pipes, int pipe_count, const char* name, int *pinned_cpu, int *priority) {
  int sock_fd;
  int retval = 0;
  struct sockaddr_un address;
  const char* pre_name = "./";
  const char* post_name = "_fd_server";
  int index, name_index = 0;
  char* actual_name;

  actual_name = (char *)malloc(sizeof(char) * (strlen(pre_name) + strlen(name) + strlen(post_name)));
  for (index = 0; index < strlen(pre_name); index++) {
    actual_name[name_index++] = pre_name[index];
  }
  for (index = 0; index < strlen(name); index++) {
    actual_name[name_index++] = name[index];
  }
  for (index = 0; index < strlen(post_name); index++) {
    actual_name[name_index++] = post_name[index];
  }
  actual_name[name_index] = 0;

  sock_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if(sock_fd < 0) {
    perror("Replica socket() failed");
    retval = -1;
    goto connect_recv_FDS_out;
  }

  /* start with a clean address structure */
  memset(&address, 0, sizeof(struct sockaddr_un));
 
  address.sun_family = AF_UNIX;
  if (strlen(actual_name) < 100) {
    memcpy(&(address.sun_path), actual_name, strlen(actual_name));
  } else {
    debug_print("Client Address length longer than max.\n");
    retval = -1;
    goto connect_recv_FDS_sock_out;
  }
  
  if(connect(sock_fd, (struct sockaddr *) &address, sizeof(struct sockaddr_un)) != 0) {
    debug_print("Replica connect() failed.\n");
    debug_print("Args: fd %d\t actual_name %s\n", sock_fd, actual_name);
    retval = -1;
    goto connect_recv_FDS_sock_out;
  }

  if (requestFDS(sock_fd, pipes, pipe_count, pinned_cpu, priority) < 0) {
    retval = -1;
    goto connect_recv_FDS_sock_out;
  }

  // Send pid
  if (write(sock_fd, &pid, sizeof(pid_t)) < 0) {
    debug_print("FD_client write for pid failed.\n");
    retval = -1;
  }

connect_recv_FDS_sock_out:
  close(sock_fd);
connect_recv_FDS_out:
  free(actual_name);
  return retval;
}
