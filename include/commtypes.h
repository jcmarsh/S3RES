/*
 * James Marshall April 30, 2014
 */

#ifndef _COMM_TYPES_H_
#define _COMM_TYPES_H_

#include <unistd.h>

// Why 10? No reason.
#define PIPE_LIMIT 10

typedef enum {
  WAY_REQ,
  WAY_RES,
  MOV_CMD,
  RANGE_POSE_DATA,
  MAP_UPDATE,
  COMM_ERROR
} comm_message_t;

// TODO: consider generating with macros: http://stackoverflow.com/questions/9907160/how-to-convert-enum-names-to-string-in-c
static const char* MESSAGE_T[] = {"WAY_REQ", "WAY_RES", "MOV_CMD", "RANGE_POSE_DATA", "MAP_UPDATE"};

#define INDEX_X 0
#define INDEX_Y 1
#define INDEX_A 2

// Not sure... each component will only have half of this pipe.
struct typed_pipe {
  comm_message_t type;
  // Only one of these will be set at a time
  int fd_in;
  int fd_out;

  int buff_count;
  char* buffer[1024];
};

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

// Coordinate sent, parameters are hard coded.
struct comm_map_update {
  int obs_x, obs_y;
  int pose_x, pose_y;
};

// Hack to check when parsing
// Also need for deserialization
comm_message_t commToEnum(char* name);

// Typed pipes
char* serializePipe(struct typed_pipe pipe);
void deserializePipe(const char* serial, struct typed_pipe* pipe);
void resetPipe(struct typed_pipe* pipe);

int commSendWaypoints(struct typed_pipe pipe, double way_x, double way_y, double way_a);
void commCopyWaypoints(struct comm_way_res * recv_msg, double * waypoints);

int commSendWaypointRequest(struct typed_pipe pipe);

int commSendMoveCommand(struct typed_pipe pipe, double vel_0, double vel_1);

int commSendMapUpdate(struct typed_pipe pipe, int obs_x, int obs_y, int pose_x, int pose_y);

int commSendRanger(struct typed_pipe pipe, double * ranger_data, double * pose_data);
void commCopyRanger(struct comm_range_pose_data * recv_msg, double * range_data, double * pose_data);

#endif // _COMM_TYPES_H_
