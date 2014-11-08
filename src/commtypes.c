#include "../include/commtypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

comm_message_t commToEnum(char* name) {
  if (strcmp(name, "WAY_RES") == 0) {
    return WAY_RES;
  } else if (strcmp(name, "WAY_REQ") == 0) {
    return WAY_REQ;
  } else if (strcmp(name, "MOV_CMD") == 0) {
    return MOV_CMD;
  } else if (strcmp(name, "RANGE_POSE_DATA") == 0) {
    return RANGE_POSE_DATA;
  } else {
    return COMM_ERROR;
  }
}

int commSendWaypoints(int send_fd, double way_x, double way_y, double way_a) {
  struct comm_way_res msg;
  memset(&msg, 0, sizeof(struct comm_way_res));

  msg.point[INDEX_X] = way_x;
  msg.point[INDEX_Y] = way_y;
  msg.point[INDEX_A] = way_a;

  return write(send_fd, &msg, sizeof(struct comm_way_res));
}

void commCopyWaypoints(struct comm_way_res * recv_msg, double * waypoints) {
  int index = 0;
  for (index = 0; index < 3; index++) {
    waypoints[index] = recv_msg->point[index];
  }
  return;
}

int commSendWaypointRequest(int send_fd) {
  struct comm_way_req send_msg;
  memset(&send_msg, 0, sizeof(struct comm_way_req));

  return write(send_fd, &send_msg, sizeof(struct comm_way_req));
}


int commSendMoveCommand(int send_fd, double vel_0, double vel_1) {
  struct comm_mov_cmd msg;
  memset(&msg, 0, sizeof(struct comm_mov_cmd));

  msg.vel_cmd[0] = vel_0;
  msg.vel_cmd[1] = vel_1;

  return write(send_fd, &msg, sizeof(struct comm_mov_cmd));
} 

int commSendRanger(int send_fd, double * ranger_data, double * pose_data) {
  int index = 0;
  struct comm_range_pose_data msg;
  memset(&msg, 0, sizeof(struct comm_range_pose_data));

  for (index = 0; index < 16; index++) {
    msg.ranges[index] = ranger_data[index];
  }
  for (index = 0; index < 3; index++) {
    msg.pose[index] = pose_data[index];
  }

  return write(send_fd, &msg, sizeof(struct comm_range_pose_data));
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
