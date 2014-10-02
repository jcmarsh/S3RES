/*
 * James Marshall April 30, 2014
 */

#ifndef _COMM_TYPES_H_
#define _COMM_TYPES_H_

#include <unistd.h>

//#define COMM_RANGE_DATA  0
//#define COMM_POS_DATA    1
#define COMM_WAY_REQ     2
#define COMM_WAY_RES     3
#define COMM_MOV_CMD     4
#define COMM_RANGE_POSE_DATA 5

#define INDEX_X 0
#define INDEX_Y 1
#define INDEX_A 2

struct comm_way_req {
  double padding[19];
};

struct comm_way_res {
  double point[3];
  double padding[16];
};

struct comm_mov_cmd {
  double vel_cmd[2];
  double padding[17];
};

struct comm_range_pose_data {
  double ranges[16];
  double pose[3];
};

struct comm_message {
  unsigned int type;
  union {
    struct comm_way_req w_req;
    struct comm_way_res w_res;
    struct comm_mov_cmd m_cmd;
    struct comm_range_pose_data rp_data;
  } data;
  unsigned short int crc; // Only used between "external" bits. For example, comm with the platform
};

int commSendWaypoints(int send_fd, double way_x, double way_y, double way_a);
void commCopyWaypoints(struct comm_message * recv_msg, double * waypoints);

int commSendWaypointRequest(int send_fd);

int commSendMoveCommand(int send_fd, double vel_0, double vel_1);

int commSendRanger(int send_fd, double * ranger_data, double * pose_data);
void commCopyRanger(struct comm_message * recv_msg, double * range_data, double * pose_data);

void generateCRC(struct comm_message *msg, unsigned short int *crc);

#endif // _COMM_TYPES_H_
