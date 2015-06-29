#include "../include/vote_buff.h"

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

void resetVotePipe(struct vote_pipe* pipe) {
  if (pipe->fd_in != 0) {
    close(pipe->fd_in);
    pipe->fd_in = 0;
  }
  if (pipe->fd_out != 0) {
    close(pipe->fd_out);
    pipe->fd_out = 0;
  }

  pipe->buff_count = 0;
  pipe->buff_index = 0;
}
