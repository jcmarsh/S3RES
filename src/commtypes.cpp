#include "../include/commtypes.h"

#define POLY 0x1021 // CRC-16-CCITT normal
void generateCRC(struct comm_message *msg, unsigned short int *crc) {
  int index, jndex;
  
  char *buffer = (char *) msg;
  int length = sizeof(msg);
  
  for (index = 0; index < length; index++) {
    char current_char = buffer[index];
      for (jndex = 0; jndex < 8; jndex++) {
        *crc = (*crc >> 1) ^ (((current_char ^ *crc) & 0x01) ? POLY : 0);
        current_char >>= 1;
    }
  }
}

int commSendWaypoints(int send_fd, double way_x, double way_y, double way_a) {
  struct comm_message msg;

  msg.type = COMM_WAY_RES;
  msg.data.w_res.point[INDEX_X] = way_x;
  msg.data.w_res.point[INDEX_Y] = way_y;
  msg.data.w_res.point[INDEX_A] = way_a;

  return write(send_fd, &msg, sizeof(struct comm_message));
}

void commCopyWaypoints(struct comm_message * recv_msg, double * waypoints) {
  int index = 0;
  for (index = 0; index < 3; index++) {
    waypoints[index] = recv_msg->data.w_res.point[index];
  }
  return;
}

int commSendWaypointRequest(int send_fd) {
  struct comm_message send_msg;
  
  send_msg.type = COMM_WAY_REQ;

  return write(send_fd, &send_msg, sizeof(struct comm_message));
}

int commSendMoveCommand(int send_fd, double vel_0, double vel_1) {
  struct comm_message msg;

  msg.type = COMM_MOV_CMD;
  msg.data.m_cmd.vel_cmd[0] = vel_0;
  msg.data.m_cmd.vel_cmd[1] = vel_1;

  return write(send_fd, &msg, sizeof(struct comm_message));
}

int commSendRanger(int send_fd, double * ranger_data, double * pose_data) {
  int index = 0;
  struct comm_message msg;
 
  msg.type = COMM_RANGE_POSE_DATA;
  for (index = 0; index < 16; index++) {
    msg.data.rp_data.ranges[index] = ranger_data[index];
  }
  for (index = 0; index < 3; index++) {
    msg.data.rp_data.pose[index] = pose_data[index];
  }

  return write(send_fd, &msg, sizeof(struct comm_message));
}

void commCopyRanger(struct comm_message * recv_msg, double * range_data, double * pose_data) {
  int index = 0;

  for (index = 0; index < 16; index++) {
    range_data[index] = recv_msg->data.rp_data.ranges[index];
  }
  for (index = 0; index < 3; index++) {
    pose_data[index] = recv_msg->data.rp_data.pose[index];
  }
  return;
}
