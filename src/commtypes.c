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

void printBuffer(struct typed_pipe *pipe) {
  printf("Print Buffer type %s, buff_count %d\n", MESSAGE_T[pipe->type], pipe->buff_count);
  int i;
  switch (pipe->type) {
    case COMM_ERROR:
      printf("Uh, that's an error type... no buffer\n");
      break;
    case WAY_REQ:
      printf("\tNo data.\n");
      break;
    case WAY_RES: ;
      struct comm_way_res *way_res = (struct comm_way_res *) pipe->buffer;
      printf("\tpoint (%f, %f) - %f\n", way_res->point[0], way_res->point[1], way_res->point[2]);
      printf("\tn_point (%f, %f) - %f\n", way_res->n_point[0], way_res->n_point[1], way_res->n_point[2]);
      break;
    case MOV_CMD: ;
      struct comm_mov_cmd *mov_cmd = (struct comm_mov_cmd *) pipe->buffer;
      printf("\tvel_cmd (%f, %f)\n", mov_cmd->vel_cmd[0], mov_cmd->vel_cmd[1]);
      break;
    case RANGE_POSE_DATA: ;
      struct comm_range_pose_data *rp_data = (struct comm_range_pose_data *) pipe->buffer;
      for (i = 0; i < RANGER_COUNT; i = i + 4) {
        printf("\tRange reading: %f %f %f %f\n", rp_data->ranges[i], rp_data->ranges[i+1], rp_data->ranges[i+2], rp_data->ranges[i+3]);
      }
      printf("\tpose (%f, %f) - %f\n", rp_data->pose[0], rp_data->pose[1], rp_data->pose[2]);
      break;
    case MAP_UPDATE: ;
      struct comm_map_update *map_update = (struct comm_map_update *) pipe->buffer;
      printf("\tpose: (%d, %d)\t Obs count: %d\n", map_update->pose_x, map_update->pose_y, map_update->obs_count);
      for (i = 0; i < map_update->obs_count; i++) {
        printf("\tObs: (%d, %d)\n", map_update->obs_x[i], map_update->obs_y[i]);
      }
      break;
    case COMM_ACK:
      printf("\tNo data.\n");
      break;
  }
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

int commSendWaypoints(struct typed_pipe pipe, 
                      double way_x, double way_y, double way_a,
                      double n_way_x, double n_way_y, double n_way_a) {
  if (pipe.fd_out == 0 || pipe.type != WAY_RES) {
    printf("commSendWaypoints Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe.type], pipe.fd_out);
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

  return TEMP_FAILURE_RETRY(write(pipe.fd_out, &msg, sizeof(struct comm_way_res)));
}

void commCopyWaypoints(struct comm_way_res * recv_msg, double * waypoints, double * n_waypoints) {
  int index = 0;
  for (index = 0; index < 3; index++) {
    waypoints[index] = recv_msg->point[index];
  }
  for (index = 0; index < 3; index++) {
    n_waypoints[index] = recv_msg->n_point[index];
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

  return TEMP_FAILURE_RETRY(write(pipe.fd_out, &send_msg, sizeof(struct comm_way_req)));
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

  return TEMP_FAILURE_RETRY(write(pipe.fd_out, &msg, sizeof(struct comm_mov_cmd)));
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
  
  int written = TEMP_FAILURE_RETRY(write(pipe.fd_out, buffer, sizeof(int) * buff_count));
  if (written != buff_count * sizeof(int)) { // TODO: more should check this
    perror("Write for commSendMapUpdate did not complete.\n");
  }

  return written;
}

// Needs to read messages one at a time, no compacting
int commRecvMapUpdate(struct typed_pipe pipe, struct comm_map_update* msg) {
  if (pipe.fd_in == 0 || pipe.type != MAP_UPDATE) {
    printf("commRecvMapUpdate Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe.type], pipe.fd_in);
    return 0;
  }
  int recv_msg_buffer[MAX_PIPE_BUFF / sizeof(int)] = {0};
  int header_ints = 3; // pose x, pose y, and obstacle count
  int index = 0;

  int read_ret = TEMP_FAILURE_RETRY(read(pipe.fd_in, &recv_msg_buffer, sizeof(int) * header_ints));
  if (read_ret == sizeof(int) * header_ints) { // TODO: Read may still have been interrupted.
    msg->pose_x = recv_msg_buffer[0];
    msg->pose_y = recv_msg_buffer[1];
    msg->obs_count = recv_msg_buffer[2];

    if (msg->obs_count > 0) { // read obstacles
      read_ret = TEMP_FAILURE_RETRY(read(pipe.fd_in, &recv_msg_buffer[header_ints], sizeof(int) * 2 * msg->obs_count));
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
        perror("commRecvMapUpdate read obstacles == 0");
      }   
    }
  } else if (read_ret > 0) {
    printf("commRecvMapUpdate read header did not match expected size: %d\n", read_ret); 
  } else if (read_ret < 0) {
    perror("commRecvMapUpdate read obstacles problems");
  } else {
    perror("commRecvMapUpdate read obstacles == 0");
  }

  return read_ret;
}

int commSendRanger(struct typed_pipe pipe, double * ranger_data, double * pose_data) {
  if (pipe.fd_out == 0 || pipe.type != RANGE_POSE_DATA) {
    printf("commSendRanger Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe.type], pipe.fd_out);
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

  int write_ret = TEMP_FAILURE_RETRY(write(pipe.fd_out, &msg, sizeof(struct comm_range_pose_data)));
  if (write_ret < sizeof(struct comm_range_pose_data)) {
    if (write_ret < 0) {
      perror("commSendRanger failed");
    } else {
      printf("commSendRange did not write expected bytes to fd %d, bytes %d\n", pipe.fd_out, write_ret);
    }
  }
  return write_ret;
}

int commSendAck(struct typed_pipe pipe, long state_hash) {
  if (pipe.fd_out == 0 || pipe.type != COMM_ACK) {
    printf("commSendAck Error: pipe type (%s) does not match type or have a valid fd (%d).\n", MESSAGE_T[pipe.type], pipe.fd_out);
    return 0;
  }

  struct comm_ack msg;
  memset(&msg, 0, sizeof(struct comm_ack));

  msg.hash = state_hash;

  return TEMP_FAILURE_RETRY(write(pipe.fd_out, &msg, sizeof(struct comm_ack)));
}

void commCopyRanger(struct comm_range_pose_data * recv_msg, double * range_data, double * pose_data) {
  int index = 0;

  for (index = 0; index < RANGER_COUNT; index++) {
    range_data[index] = recv_msg->ranges[index];
  }
  for (index = 0; index < 3; index++) {
    pose_data[index] = recv_msg->pose[index];
  }
  return;
}
