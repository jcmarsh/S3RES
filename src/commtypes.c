#include "../include/commtypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TYPED_PIPE_BUFF 4096 // This should be the limit kernel on pipes... or something reasonable.

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

char* serializePipe(struct typed_pipe pipe) {
  char* serial;
  if (pipe.fd_in == 0) {
    if (asprintf(&serial, "%s:%d:%d", MESSAGE_T[pipe.type], 0, pipe.fd_out) < 0) {
      perror("serializePipe failed");
    }
  } else {
    if (asprintf(&serial, "%s:%d:%d", MESSAGE_T[pipe.type], pipe.fd_in, 0) < 0) {
      perror("serializePipe failed");
    }
  }
  return serial;  
}

void deserializePipe(const char* serial, struct typed_pipe* pipe) {
  char *type;
  int in = 0, out = 0;

  // TODO: check allocation and scan for errors
  sscanf(serial, "%m[^:]:%d:%d", &type, &in, &out);
  pipe->fd_in = in;
  pipe->fd_out = out;

  pipe->type = commToEnum(type);
  free(type);
}

void resetPipe(struct typed_pipe* pipe) {
  pipe->type = COMM_ERROR;
  if (pipe->fd_in != 0) {
    close(pipe->fd_in);
    pipe->fd_in = 0;
  }
  if (pipe->fd_out != 0) {
    close(pipe->fd_out);
    pipe->fd_out = 0;
  }
}

void convertTypedToVote(struct typed_pipe ext_pipes[], int pipe_count, struct vote_pipe *new_pipes) {
  int i;
  for (i = 0; i < pipe_count; i++) {
    new_pipes[i].fd_in = ext_pipes[i].fd_in;
    new_pipes[i].fd_out = ext_pipes[i].fd_out;
    new_pipes[i].rep_info = (char *)MESSAGE_T[ext_pipes[i].type];
  }
}

void convertVoteToTyped(struct vote_pipe ext_pipes[], int pipe_count, struct typed_pipe *new_pipes) {
  int i;
  for (i = 0; i < pipe_count; i++) {
    new_pipes[i].fd_in = ext_pipes[i].fd_in;
    new_pipes[i].fd_out = ext_pipes[i].fd_out;
    new_pipes[i].type = commToEnum(ext_pipes[i].rep_info);
  }
}

void printBuffer(struct typed_pipe* pipe, char *buffer, int buff_count) {
  printf("Print Buffer type %s, buff_count %d\n", MESSAGE_T[pipe->type], buff_count);

  int i;
  switch (pipe->type) {
    case COMM_ERROR:
      printf("Uh, that's an error type... no buffer\n");
      break;
    case WAY_REQ:
      printf("\tNo data.\n");
      break;
    case WAY_RES: ;
      struct comm_way_res *way_res = (struct comm_way_res *) buffer;
      printf("\tpoint (%f, %f) - %f\n", way_res->point[0], way_res->point[1], way_res->point[2]);
      printf("\tn_point (%f, %f) - %f\n", way_res->n_point[0], way_res->n_point[1], way_res->n_point[2]);
      break;
    case MOV_CMD: ;
      struct comm_mov_cmd *mov_cmd = (struct comm_mov_cmd *) buffer;
      printf("\tvel_cmd (%f, %f)\n", mov_cmd->vel_cmd[0], mov_cmd->vel_cmd[1]);
      break;
    case RANGE_POSE_DATA: ;
      struct comm_range_pose_data *rp_data = (struct comm_range_pose_data *) buffer;
      for (i = 0; i < RANGER_COUNT; i = i + 4) {
        printf("\tRange reading: %f %f %f %f\n", rp_data->ranges[i], rp_data->ranges[i+1], rp_data->ranges[i+2], rp_data->ranges[i+3]);
      }
      printf("\tpose (%f, %f) - %f\n", rp_data->pose[0], rp_data->pose[1], rp_data->pose[2]);
      break;
    case MAP_UPDATE: ;
      int header_ints = 3; // pose x, pose y, and obstacle count
      struct comm_map_update *map_update = (struct comm_map_update *) buffer;
      printf("\tpose: (%d, %d)\t Obs count: %d\n", map_update->pose_x, map_update->pose_y, map_update->obs_count);
      for (i = 0; i < map_update->obs_count; i++) {
        printf("\tObs: (%d, %d)\n", buffer[header_ints + (i *2)], buffer[header_ints + (i *2 + 1)]);//map_update->obs_x[i], map_update->obs_y[i]);
      }
      break;
    case COMM_ACK: ;
      struct comm_ack *ack = (struct comm_ack *) buffer;
      printf("\tHash: %08lx\n", ack->hash);
      break;
    case MSG_BUFFER:
      printf("\tNot implemented.\n");
      break;
    default:
      printf("\tUnknown type.\n");
      break;
  }
}

int commSendWaypoints(struct typed_pipe* pipe, 
                      double way_x, double way_y, double way_a,
                      double n_way_x, double n_way_y, double n_way_a) {
  if (pipe->fd_out == 0 || pipe->type != WAY_RES) {
    printf("commSendWaypoints Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe->type], pipe->fd_out);
    return 0;
  }

  struct comm_way_res msg;
  memset(&msg, 0, sizeof(struct comm_way_res));

  msg.point[INDEX_X] = way_x;
  msg.point[INDEX_Y] = way_y;
  msg.point[INDEX_A] = way_a;

  msg.n_point[INDEX_X] = n_way_x;
  msg.n_point[INDEX_Y] = n_way_y;
  msg.n_point[INDEX_A] = n_way_a;

  return write(pipe->fd_out, &msg, sizeof(struct comm_way_res));
}

void commCopyWaypoints(struct comm_way_res* recv_msg, double* waypoints, double* n_waypoints) {
  int index = 0;
  for (index = 0; index < 3; index++) {
    waypoints[index] = recv_msg->point[index];
  }
  for (index = 0; index < 3; index++) {
    n_waypoints[index] = recv_msg->n_point[index];
  }

  return;
}

int commSendWaypointRequest(struct typed_pipe* pipe) {
  if (pipe->fd_out == 0 || pipe->type != WAY_REQ) {
    printf("commSendWaypointsRequest Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe->type], pipe->fd_out);
    return 0;
  }

  struct comm_way_req send_msg;
  memset(&send_msg, 0, sizeof(struct comm_way_req));

  return write(pipe->fd_out, &send_msg, sizeof(struct comm_way_req));
}


int commSendMoveCommand(struct typed_pipe* pipe, double vel_0, double vel_1) {
  if (pipe->fd_out == 0 || pipe->type != MOV_CMD) {
    printf("commSendMoveCommand Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe->type], pipe->fd_out);
    return 0;
  }

  struct comm_mov_cmd msg;
  memset(&msg, 0, sizeof(struct comm_mov_cmd));

  msg.vel_cmd[0] = vel_0;
  msg.vel_cmd[1] = vel_1;

  return write(pipe->fd_out, &msg, sizeof(struct comm_mov_cmd));
}

int commSendMapUpdate(struct typed_pipe* pipe, struct comm_map_update* msg) {
  if (pipe->fd_out == 0 || pipe->type != MAP_UPDATE) {
    printf("commSendMapUpdate Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe->type], pipe->fd_out);
    return 0;
  }

  int index = 0;
  int buffer[MAX_TYPED_PIPE_BUFF / sizeof(int)] = {0};
  int buff_count = 0;

  buffer[buff_count++] = msg->pose_x;
  buffer[buff_count++] = msg->pose_y;
  buffer[buff_count++] = msg->obs_count;

  for (index = 0; index < msg->obs_count; index++) {
    buffer[buff_count++] = msg->obs_x[index];
    buffer[buff_count++] = msg->obs_y[index];
    
    if (buff_count * sizeof(int) > MAX_TYPED_PIPE_BUFF) {
      printf("ERROR: Commtypes:commSendMapUpdate attempting to surpase MAX_TYPED_PIPE_BUFF\n");
      break;
    }
  }
  
  int written = write(pipe->fd_out, buffer, sizeof(int) * buff_count);
  if (written != buff_count * sizeof(int)) { // TODO: more should check this
    perror("Write for commSendMapUpdate did not complete.\n");
  }

  return written;
}

// Needs to read messages one at a time, no compacting
int commRecvMapUpdate(struct typed_pipe* pipe, struct comm_map_update* msg) {
  if (pipe->fd_in == 0 || pipe->type != MAP_UPDATE) {
    printf("commRecvMapUpdate Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe->type], pipe->fd_in);
    return 0;
  }
  int recv_msg_buffer[MAX_TYPED_PIPE_BUFF / sizeof(int)] = {0};
  int header_ints = 3; // pose x, pose y, and obstacle count
  int index = 0;

  int read_ret = read(pipe->fd_in, &recv_msg_buffer, sizeof(int) * header_ints);
  if (read_ret == sizeof(int) * header_ints) { // TODO: Read may still have been interrupted.
    msg->pose_x = recv_msg_buffer[0];
    msg->pose_y = recv_msg_buffer[1];
    msg->obs_count = recv_msg_buffer[2];

    if (msg->obs_count > 0) { // read obstacles
      read_ret = read(pipe->fd_in, &recv_msg_buffer[header_ints], sizeof(int) * 2 * msg->obs_count);
      if (read_ret == sizeof(int) * 2 * msg->obs_count) {
        for (index = 0; index < msg->obs_count; index++) {
          msg->obs_x[index] = recv_msg_buffer[header_ints + (index * 2)];
          msg->obs_y[index] = recv_msg_buffer[header_ints + (index * 2 + 1)];
        }
      } else if (read_ret > 0) {
        printf("commRecvMapUpdate read obstacles did not match expected size: %d\n", read_ret); 
      } else if (read_ret < 0) {
        perror("commRecvMapUpdate read obstacles problems");
      } else {
        perror("commRecvMapUpdate inner read obstacles == 0");
      }   
    }
  } else if (read_ret > 0) {
    printf("commRecvMapUpdate read header did not match expected size: %d\n", read_ret); 
  } else if (read_ret < 0) {
    perror("commRecvMapUpdate read obstacles problems");
  } else {
    perror("commRecvMapUpdate outer read obstacles == 0");
  }

  return read_ret;
}

int commSendRanger(struct typed_pipe* pipe, double* ranger_data, double* pose_data) {
  if (pipe->fd_out == 0 || pipe->type != RANGE_POSE_DATA) {
    printf("commSendRanger Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe->type], pipe->fd_out);
    return 0;
  }

  int index = 0;

  struct comm_range_pose_data msg;
  memset(&msg, 0, sizeof(struct comm_range_pose_data));

  for (index = 0; index < RANGER_COUNT; index++) {
    msg.ranges[index] = ranger_data[index];
  }
  for (index = 0; index < 3; index++) {
    msg.pose[index] = pose_data[index];
  }

  int write_ret = write(pipe->fd_out, &msg, sizeof(struct comm_range_pose_data));
  if (write_ret < sizeof(struct comm_range_pose_data)) {
    if (write_ret < 0) {
      perror("commSendRanger failed");
    } else {
      printf("commSendRange did not write expected bytes to fd %d, bytes %d\n", pipe->fd_out, write_ret);
    }
  }

  return write_ret;
}

int commSendAck(struct typed_pipe* pipe, long state_hash) {
  if (pipe->fd_out == 0 || pipe->type != COMM_ACK) {
    printf("commSendAck Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe->type], pipe->fd_out);
    return 0;
  }

  struct comm_ack msg;
  memset(&msg, 0, sizeof(struct comm_ack));

  msg.hash = state_hash;

  return write(pipe->fd_out, &msg, sizeof(struct comm_ack));
}

void commCopyRanger(struct comm_range_pose_data* recv_msg, double* range_data, double* pose_data) {
  int index = 0;

  for (index = 0; index < RANGER_COUNT; index++) {
    range_data[index] = recv_msg->ranges[index];
  }
  for (index = 0; index < 3; index++) {
    pose_data[index] = recv_msg->pose[index];
  }

  return;
}

// TODO: Consider making this smarter so that the sender doesn't have to do as much work.
int commSendMsgBuffer(struct typed_pipe* pipe, struct comm_msg_buffer* msg) {
  if (pipe->fd_out == 0 || pipe->type != MSG_BUFFER) {
    printf("commSendMsgBuffer Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe->type], pipe->fd_out);
    return 0;
  }
  
  char *buffer = (char *) malloc(sizeof(int) + sizeof(char) * msg->length);
  int i;
  for (i = 0; i < (int) sizeof(int); i++) {
    buffer[i] = ((char *)(&(msg->length)))[i];
  }

  memcpy(&(buffer[4]), msg->message, msg->length);

  int written = write(pipe->fd_out, &(buffer[0]), msg->length + sizeof(msg->length));
  if (written != msg->length + sizeof(msg->length)) { // TODO: more should check this
    perror("Write for commSendMsgBuffer did not complete.\n");
  }

  return written;
}

int commRecvMsgBuffer(struct typed_pipe* pipe, struct comm_msg_buffer* msg) {
  if (pipe->fd_in == 0 || pipe->type != MSG_BUFFER) {
    printf("commRecvMsgBuffer Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe->type], pipe->fd_in);
    return 0;
  }

  int read_ret = read(pipe->fd_in, &(msg->length), sizeof(msg->length));
  if (read_ret == sizeof(msg->length)) { // TODO: Read may still have been interrupted.
    msg->message = (char*) malloc(sizeof(char) * msg->length);

    if (msg->length > 0) {
      read_ret = read(pipe->fd_in, msg->message, sizeof(char) * msg->length);
      if (read_ret == msg->length) {
        // All good
      } else if (read_ret > 0) {
        printf("commRecvMsgBuffer read buffer count (%d) did not match expected size (%d)\n", read_ret, msg->length); 
      } else if (read_ret < 0) {
        perror("commRecvMsgBuffer read buffer problems");
      } else {
        perror("commRecvMsgBuffer inner read buffer == 0");
      }   
    }
  } else if (read_ret > 0) {
    printf("commRecvMsgBuffer read length did not match expected size: %d\n", read_ret); 
  } else if (read_ret < 0) {
    perror("commRecvMsgBuffer outer read buffer problems");
  } else {
    perror("commRecvMsgBuffer outer read buffer == 0");
  }

  return read_ret;
}
