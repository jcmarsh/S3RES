/*
 * James Marshall April 30, 2014
 */

#ifndef _COMM_TYPES_H_
#define _COMM_TYPES_H_

#define COMM_RANGE_DATA  0
#define COMM_POS_DATA    1
#define COMM_WAY_REQ     2
#define COMM_WAY_RES     3
#define COMM_MOV_CMD     4

#define INDEX_X 0
#define INDEX_Y 1
#define INDEX_A 2

struct comm_header {
  uint type;
  uint byte_count; // Byte count of the rest of the message!
};

struct comm_range_data_msg {
  struct comm_header hdr;
  double ranges[16];
};

struct comm_pos_data_msg {
  struct comm_header hdr;
  double pose[3];
};

// no struct need for comm_way_req: carries no data

struct comm_way_res_msg {
  struct comm_header hdr;
  double point[3];
};

struct comm_mov_cmd_msg {
  struct comm_header hdr;
  double vel_cmd[2];
};

#endif // _COMM_TYPES_H_
