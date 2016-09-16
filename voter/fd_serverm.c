/*
 * fd_serverm.c
 *
 * Modified from fd_server.c to accomodate the simplified VoterM.
 *
 * James Marshall Feb 9 2016
 */

#include "voterm.h"

// Copied from commtypes
typedef enum {
  COMM_ERROR,
  WAY_REQ,
  WAY_RES,
  MOV_CMD,
  RANGE_POSE_DATA,
  MAP_UPDATE,
  COMM_ACK,
  MSG_BUFFER,
} comm_message_t;

// Copied from commtypes
comm_message_t commToEnum(char* name) {
  if (strcmp(name, "WAY_REQ") == 0) {
    return WAY_REQ;
  } else if (strcmp(name, "WAY_RES") == 0) {
    return WAY_RES;
  } else if (strcmp(name, "MOV_CMD") == 0) {
    return MOV_CMD;
  } else if (strcmp(name, "RANGE_POSE_DATA") == 0) {
    return RANGE_POSE_DATA;
  } else if (strcmp(name, "MAP_UPDATE") == 0) {
    return MAP_UPDATE;
  } else if (strcmp(name, "COMM_ACK") == 0) {
    return COMM_ACK;
  } else if (strcmp(name, "MSG_BUFFER") == 0) {
    return MSG_BUFFER;
  } else {
    return COMM_ERROR;
  }
}

// Copied from commtypes
struct typed_pipe {
  comm_message_t type;

  // Only one of these will be set at a time
  int fd_in;
  int fd_out;
};

// rep_info_in and rep_info_out needed.
int sendFDS(int connection_fd, struct replicaR * rep, char **rep_info_in, char **rep_info_out, int pinned_cpu, int priority) { // pipes are the rep side
  int i, p_index = 0;
  int pipe_count = rep->in_pipe_count + rep->out_pipe_count;
  struct msghdr hdr;
  struct iovec datas[3];

  // cmsg is the out-of-band data (fds)
  char cmsgbuf[CMSG_SPACE(sizeof(int) * (pipe_count))];

  struct typed_pipe send_pipes[pipe_count];
  // convertVoteToTyped(pipes, pipe_count, send_pipes);
  for (i = 0; i < rep->in_pipe_count; i++) {
    send_pipes[p_index].fd_in = rep->fd_ins[i];
    send_pipes[p_index].fd_out = 0;
    send_pipes[p_index].type = commToEnum(rep_info_in[i]);
    p_index++;
  }
  for (i = 0; i < rep->out_pipe_count; i++) {
    send_pipes[p_index].fd_in = 0;
    send_pipes[p_index].fd_out = rep->fd_outs[i];;
    send_pipes[p_index].type = commToEnum(rep_info_out[i]);
    p_index++;
  }

  datas[0].iov_base = send_pipes;
  datas[0].iov_len = sizeof(send_pipes);

  datas[1].iov_base = &pinned_cpu;
  datas[1].iov_len = sizeof(pinned_cpu);

  datas[2].iov_base = &priority;
  datas[2].iov_len = sizeof(priority);

  memset(&hdr, 0, sizeof(hdr));
  hdr.msg_name = NULL;
  hdr.msg_namelen = 0;
  hdr.msg_iov = datas;
  hdr.msg_iovlen =  3; // number of iovec items in datas
  hdr.msg_flags = 0;

  hdr.msg_control = cmsgbuf;
  hdr.msg_controllen = CMSG_LEN(sizeof(int) * pipe_count);

  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&hdr);
  cmsg->cmsg_len   = CMSG_LEN(sizeof(int) * pipe_count);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type  = SCM_RIGHTS;

  p_index = 0;
  for (i = 0; i < rep->in_pipe_count; i++) {
    ((int*)CMSG_DATA(cmsg))[p_index] = rep->fd_ins[i];
    p_index++;
  }
  for (i = 0; i < rep->out_pipe_count; i++) {
    ((int*)CMSG_DATA(cmsg))[p_index] = rep->fd_outs[i];
    p_index++;
  }

  int retval = sendmsg(connection_fd, &hdr, 0);
  if(retval < 0) {
    debug_print("FD_ServerR sendmsg() failed.\n");
  }

  return retval;
}

// Name is ./<name>_fd_server
int createFDS(struct server_data * sd, const char* name) {
  // create domain socked
  const char* pre_name = "./";
  const char* post_name = "_fd_server";
  int index, name_index = 0;
  char* actual_name;
  
  actual_name = (char *)malloc(sizeof(char) * (strlen(pre_name) + strlen(name) + strlen(post_name) + 1));
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

  sd->sock_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (sd->sock_fd < 0) {
    debug_print("FD_ServerR socket creation failed.\n");
    return 1;
  }

  // should delete previous remnants
  unlink(actual_name);

  /* start with a clean address structure */
  memset(&(sd->address), 0, sizeof(struct sockaddr_un));

  sd->address.sun_family = AF_UNIX;
  if (strlen(actual_name) < 200) { // UNIX_PATH_MAX) {
    memcpy(&(sd->address.sun_path), actual_name, strlen(actual_name));
  } else {
    debug_print("FD_ServerR server Address length longer than max.\n");
    return -1;
  }
  free(actual_name);

  if(bind(sd->sock_fd, (struct sockaddr *) &(sd->address), sizeof(struct sockaddr_un)) != 0) {
    debug_print("FD_ServerR bind() failed\n");
    return 1;
  }

  // This program will listen to connections on this socket
  // 5 is the backlog: how many pending connections there may be
  if(listen(sd->sock_fd, 5) != 0) {
    debug_print("FD_ServerR listen() failed\n");
    return 1;
  }
}

/*
 * 
 * Blocks on accept: You better know a client is about to connect!
 * Returns 0 upon success, <0 otherwise.
 */
int acceptSendFDS(struct server_data * sd, struct replicaR * rep, char **rep_info_in, char **rep_info_out) {
  int connection_fd;
  int read_ret, retval = 0;

  connection_fd = accept(sd->sock_fd, (struct sockaddr *) &(sd->address), &(sd->address_length));
  if (connection_fd > -1) {
    // send read end to client
    if (sendFDS(connection_fd, rep, rep_info_in, rep_info_out, rep->pinned_cpu, rep->priority) < 0) {
      debug_print("FD_ServerR failed to sendFDS.\n");
      retval = -1;
      goto accept_send_FDS_out;
    }
  } else {
    debug_print("FD_ServerR failed to accept.\n");
    retval = -1;
    goto accept_send_FDS_out;
  }

  // read pid
  read_ret = read(connection_fd, &retval, sizeof(pid_t));
  if (read_ret != sizeof(pid_t)) {
    debug_print("FD_ServerR failed to read pid.\n");
    retval = -1;
  }

accept_send_FDS_out:
  close(connection_fd);
  return retval;
}
