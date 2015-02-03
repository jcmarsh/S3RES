/*
 * James Marshall April 30, 2014
 */

#ifndef _COMM_TYPES_H_
#define _COMM_TYPES_H_

#include <stdbool.h>
#include <unistd.h>

// Why 10? No reason.
#define PIPE_LIMIT 10

typedef enum {
  COMM_ERROR,
  WAY_REQ,
  WAY_RES,
  MOV_CMD,
  RANGE_POSE_DATA,
  MAP_UPDATE,
  COMM_ACK
} comm_message_t;

// Not happy with this, but clean. Order needs to be the same as above.
// Used to serialize / deserialize the pipe type
static const char* MESSAGE_T[] = {"COMM_ERROR", "WAY_REQ", "WAY_RES", "MOV_CMD", "RANGE_POSE_DATA", "MAP_UPDATE", "COMM_ACK"};

// Here more for convenience
typedef enum {NONE, SMR, DMR, TMR, REP_TYPE_ERROR} replication_t;
static const char* REP_TYPE_T[] = {"NONE", "SMR", "DMR", "TMR", "REP_TYPE_ERROR"};

#define INDEX_X 0
#define INDEX_Y 1
#define INDEX_A 2

// Not sure... each component will only have half of this pipe.
struct typed_pipe {
  comm_message_t type;

  bool timed; // timers start on input timed pipe reads, reset on output timed pipe writes
  // Only one of these will be set at a time
  int fd_in;
  int fd_out;

  int buff_count;
  char* buffer[1024];
};

struct comm_way_req {
  int padding;
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
  int pose_x, pose_y;
  int obs_count;
  int* obs_x;
  int* obs_y;
};

struct comm_ack {
  int padding;
};

// Hack to check when parsing
// Also need for deserialization
comm_message_t commToEnum(char* name);
replication_t reptypeToEnum(char* type);

// Typed pipes
char* serializePipe(struct typed_pipe pipe);
void deserializePipe(const char* serial, struct typed_pipe* pipe);
void resetPipe(struct typed_pipe* pipe);

int commSendWaypoints(struct typed_pipe pipe, double way_x, double way_y, double way_a);
void commCopyWaypoints(struct comm_way_res * recv_msg, double * waypoints);

int commSendWaypointRequest(struct typed_pipe pipe);

int commSendMoveCommand(struct typed_pipe pipe, double vel_0, double vel_1);

int commSendMapUpdate(struct typed_pipe pipe, struct comm_map_update* msg);
//void commCopyMapUpdate(struct comm_map_update* recv_msg, int* pose_x, int* pose_y, int, )

int commSendRanger(struct typed_pipe pipe, double * ranger_data, double * pose_data);
void commCopyRanger(struct comm_range_pose_data * recv_msg, double * range_data, double * pose_data);

int commSendAck(struct typed_pipe pipe);
#endif // _COMM_TYPES_H_
