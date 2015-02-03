#include "../include/commtypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  } else {
    return COMM_ERROR;
  }
}

replication_t reptypeToEnum(char* type) {
  if (strcmp(type, "NONE") == 0) {
    return NONE;
  } else if (strcmp(type, "SMR") == 0) {
    return SMR;
  } else if (strcmp(type, "DMR") == 0) {
    return DMR;
  } else if (strcmp(type, "TMR") == 0) {
    return TMR;
  } else {
    return REP_TYPE_ERROR;
  }
}

char* serializePipe(struct typed_pipe pipe) {
  char* serial;
  if (pipe.fd_in == 0) {
    if (asprintf(&serial, "%s:%d:%d:%d", MESSAGE_T[pipe.type], 0, pipe.fd_out, pipe.timed) < 0) {
      perror("serializePipe failed");
    }
  } else {
    if (asprintf(&serial, "%s:%d:%d:%d", MESSAGE_T[pipe.type], pipe.fd_in, 0, pipe.timed) < 0) {
      perror("serializePipe failed");
    }
  }
  return serial;  
}

void deserializePipe(const char* serial, struct typed_pipe *pipe) {
  char* type;
  int in = 0;
  int out = 0;
  int timed = 0;

  // TODO: check allocation and scan for errors
  sscanf(serial, "%m[^:]:%d:%d:%d", &type, &in, &out, &timed);
  pipe->fd_in = in;
  pipe->fd_out = out;
  pipe->timed = timed;

  pipe->type = commToEnum(type);

  memset(pipe->buffer, 0, MAX_PIPE_BUFF);
  pipe->buff_count = 0;

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

int commSendWaypoints(struct typed_pipe pipe, double way_x, double way_y, double way_a) {
  if (pipe.fd_out == 0 || pipe.type != WAY_RES) {
    printf("commSendWaypoints Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe.type], pipe.fd_out);
    return 0;
  }

  struct comm_way_res msg;
  memset(&msg, 0, sizeof(struct comm_way_res));

  msg.point[INDEX_X] = way_x;
  msg.point[INDEX_Y] = way_y;
  msg.point[INDEX_A] = way_a;

  return write(pipe.fd_out, &msg, sizeof(struct comm_way_res));
}

void commCopyWaypoints(struct comm_way_res * recv_msg, double * waypoints) {
  int index = 0;
  for (index = 0; index < 3; index++) {
    waypoints[index] = recv_msg->point[index];
  }
  return;
}

int commSendWaypointRequest(struct typed_pipe pipe) {
  if (pipe.fd_out == 0 || pipe.type != WAY_REQ) {
    printf("commSendWaypointsRequest Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe.type], pipe.fd_out);
    return 0;
  }

  struct comm_way_req send_msg;
  memset(&send_msg, 0, sizeof(struct comm_way_req));

  return write(pipe.fd_out, &send_msg, sizeof(struct comm_way_req));
}


int commSendMoveCommand(struct typed_pipe pipe, double vel_0, double vel_1) {
  if (pipe.fd_out == 0 || pipe.type != MOV_CMD) {
    printf("commSendMoveCommand Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe.type], pipe.fd_out);
    return 0;
  }

  struct comm_mov_cmd msg;
  memset(&msg, 0, sizeof(struct comm_mov_cmd));

  msg.vel_cmd[0] = vel_0;
  msg.vel_cmd[1] = vel_1;

  return write(pipe.fd_out, &msg, sizeof(struct comm_mov_cmd));
}

int commSendMapUpdate(struct typed_pipe pipe, struct comm_map_update* msg) {
  if (pipe.fd_out == 0 || pipe.type != MAP_UPDATE) {
    printf("commSendMapUpdate Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe.type], pipe.fd_out);
    return 0;
  }

  int index = 0;
  int buffer[MAX_PIPE_BUFF / sizeof(int)] = {0};
  int buff_count = 0;

  buffer[buff_count++] = msg->pose_x;
  buffer[buff_count++] = msg->pose_y;
  buffer[buff_count++] = msg->obs_count;

  for (index = 0; index < msg->obs_count; index++) {
    buffer[buff_count++] = msg->obs_x[index];
    buffer[buff_count++] = msg->obs_y[index];
    
    if (buff_count * sizeof(int) > MAX_PIPE_BUFF) {
      printf("ERROR: Commtypes:commSendMapUpdate attempting to surpase MAX_PIPE_BUFF\n");
      break;
    }
  }
  
  return write(pipe.fd_out, buffer, sizeof(int) * buff_count);
}

int commSendRanger(struct typed_pipe pipe, double * ranger_data, double * pose_data) {
  if (pipe.fd_out == 0 || pipe.type != RANGE_POSE_DATA) {
    printf("commSendRanger Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe.type], pipe.fd_out);
    return 0;
  }

  int index = 0;

  struct comm_range_pose_data msg;
  memset(&msg, 0, sizeof(struct comm_range_pose_data));

  for (index = 0; index < 16; index++) {
    msg.ranges[index] = ranger_data[index];
  }
  for (index = 0; index < 3; index++) {
    msg.pose[index] = pose_data[index];
  }

  return write(pipe.fd_out, &msg, sizeof(struct comm_range_pose_data));
}

int commSendAck(struct typed_pipe pipe) {
  if (pipe.fd_out == 0 || pipe.type != COMM_ACK) {
    printf("commSendAck Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe.type], pipe.fd_out);
    return 0;
  }

  struct comm_ack msg;
  memset(&msg, 0, sizeof(struct comm_ack));

  return write(pipe.fd_out, &msg, sizeof(struct comm_ack));
}

void commCopyRanger(struct comm_range_pose_data * recv_msg, double * range_data, double * pose_data) {
  int index = 0;

  for (index = 0; index < 16; index++) {
    range_data[index] = recv_msg->ranges[index];
  }
  for (index = 0; index < 3; index++) {
    pose_data[index] = recv_msg->pose[index];
  }
  return;
}
