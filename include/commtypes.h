/*
 * James Marshall April 30, 2014
 */

#ifndef _COMM_TYPES_H_
#define _COMM_TYPES_H_

#define _GNU_SOURCE 1

#include <stdbool.h>
#include <unistd.h>

#include "bench_config.h"
#include "vote_buff.h"
 
// Number of range sensors
#define RANGER_COUNT 16

typedef enum {
  COMM_ERROR,
  WAY_REQ,
  WAY_RES,
  MOV_CMD,
  RANGE_POSE_DATA,
  MAP_UPDATE,
  COMM_ACK,
  MSG_BUFFER,
} comm_message_t;

// Not happy with this, but clean. Order needs to be the same as above.
// Used to serialize / deserialize the pipe type
static const char* MESSAGE_T[] = {"COMM_ERROR", "WAY_REQ", "WAY_RES", "MOV_CMD", "RANGE_POSE_DATA", "MAP_UPDATE", "COMM_ACK", "MSG_BUFFER"};

#define INDEX_X 0
#define INDEX_Y 1
#define INDEX_A 2

// Not sure... each component will only have half of this pipe.
struct typed_pipe {
  comm_message_t type;

  // Only one of these will be set at a time
  int fd_in;
  int fd_out;
};

struct comm_way_req {
  int padding;
};

struct comm_way_res {
  double point[3];
  double n_point[3];
};

struct comm_mov_cmd {
  double vel_cmd[2];
};

struct comm_range_pose_data {
  double ranges[RANGER_COUNT];
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
  long hash;
};

struct comm_msg_buffer {
  int length;
  char* message;
};

// Hack to check when parsing
// Also need for deserialization
comm_message_t commToEnum(char* name);

// void printBuffer(struct typed_pipe* pipe);

// Typed pipes
char* serializePipe(struct typed_pipe pipe);
void deserializePipe(const char* serial, struct typed_pipe* pipe);
void resetPipe(struct typed_pipe* pipe);

// Used in benchmarker and fd_server
void convertTypedToVote(struct typed_pipe ext_pipes[], int pipe_count, struct vote_pipe *new_pipes);
void convertVoteToTyped(struct vote_pipe ext_pipes[], int pipe_count, struct typed_pipe *new_pipes);

int commSendWaypoints(struct typed_pipe* pipe,
                      double way_x, double way_y, double way_a,
                      double n_way_x, double n_way_y, double n_way_a);
void commCopyWaypoints(struct comm_way_res* recv_msg, double* waypoints, double* n_waypoints);

int commSendWaypointRequest(struct typed_pipe* pipe);

int commSendMoveCommand(struct typed_pipe* pipe, double vel_0, double vel_1);

int commSendMapUpdate(struct typed_pipe* pipe, struct comm_map_update* msg);
int commRecvMapUpdate(struct typed_pipe* pipe, struct comm_map_update* msg);

int commSendRanger(struct typed_pipe* pipe, double* ranger_data, double* pose_data);
void commCopyRanger(struct comm_range_pose_data* recv_msg, double* range_data, double* pose_data);

int commSendAck(struct typed_pipe* pipe, long state_hash);

int commSendMsgBuffer(struct typed_pipe* pipe, struct comm_msg_buffer* msg); // If I don't stipulate null terminated, these message can pass arbitrary buffers, which might be nice
int commRecvMsgBuffer(struct typed_pipe* pipe, struct comm_msg_buffer* msg);

#endif // _COMM_TYPES_H_
