/*
 * James Marshall April 30, 2014
 */

#ifndef _COMM_TYPES_H_
#define _COMM_TYPES_H_

#include <unistd.h>

typedef enum {
  WAY_REQ,
  WAY_RES,
  MOV_CMD,
  RANGE_POSE_DATA,
  COMM_ERROR
} comm_message_t;

#define INDEX_X 0
#define INDEX_Y 1
#define INDEX_A 2

struct comm_way_req {
  double padding;
};

struct comm_way_res {
  double point[3];
};

struct comm_mov_cmd {
  double vel_cmd[2];
};

struct comm_range_pose_data {
  double ranges[16];
  double pose[3];
};

// Hack to check when parsing
comm_message_t commToEnum(char* name);

int commSendWaypoints(int send_fd, double way_x, double way_y, double way_a);
void commCopyWaypoints(struct comm_way_res * recv_msg, double * waypoints);

int commSendWaypointRequest(int send_fd);

int commSendMoveCommand(int send_fd, double vel_0, double vel_1);

int commSendRanger(int send_fd, double * ranger_data, double * pose_data);
void commCopyRanger(struct comm_range_pose_data * recv_msg, double * range_data, double * pose_data);

#endif // _COMM_TYPES_H_
