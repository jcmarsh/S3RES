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
  uint byte_count;
};

#endif // _COMM_TYPES_H_
