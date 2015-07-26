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

// read available bytes into the buffer. Return -1 if size exceeded.
int pipeToBuff(struct vote_pipe* pipe) {
  char temp_buffer[MAX_VOTE_PIPE_BUFF];
  int read_count;

  read_count = TEMP_FAILURE_RETRY(read(pipe->fd_in, temp_buffer, MAX_VOTE_PIPE_BUFF));

  if (read_count > 0) {
    if (read_count < MAX_VOTE_PIPE_BUFF - pipe->buff_count) {
      // Space for the data: copy it to the vote_buff
      int end_index = (pipe->buff_index + pipe->buff_count) % MAX_VOTE_PIPE_BUFF;
      if (end_index > (end_index + read_count) % MAX_VOTE_PIPE_BUFF) {
        // need to wrap back to begining, two copies
        int part_read = read_count - ((end_index + read_count) % MAX_VOTE_PIPE_BUFF);
        memcpy(&(pipe->buffer[end_index]), &(temp_buffer[0]), part_read);
        memcpy(&(pipe->buffer[0]), &(temp_buffer[part_read]), read_count - part_read);
      } else {
        // simple case: just copy
        memcpy(&(pipe->buffer[end_index]), &(temp_buffer[0]), read_count);
      }
      pipe->buff_count += read_count;
    } else {
      printf("Vote_buff overflow!!!\n");
      return -1;
    }
  } else {
    printf("Vote_buff read error.\n");
    return -1;
  }

  return 0;
}

// write n bytes
int buffToPipe(struct vote_pipe* pipe, int fd_out, int n) {
  // Just needs to write out the data! But may need two writes...
  if (n > pipe->buff_count) {
    printf("Vote_buff buffToPipe: requested bigger write than available\n");
    return -1;
  }

  if (pipe->buff_index + n < MAX_VOTE_PIPE_BUFF) { // simple case: just write
    TEMP_FAILURE_RETRY(write(fd_out, &(pipe->buffer[pipe->buff_index]), n));
  } else {
    // memcpy to buffer, then write out.
    char temp_buffer[n];

    int part_write = MAX_VOTE_PIPE_BUFF - pipe->buff_index;
    memcpy(&(temp_buffer[0]), &(pipe->buffer[pipe->buff_index]), part_write);
    memcpy(&(temp_buffer[part_write]), &(pipe->buffer[0]), n - part_write);

    TEMP_FAILURE_RETRY(write(fd_out, temp_buffer, n));
  }

  fakeToPipe(pipe, n);
  return 0;
}

// Advances buffer, but no need to write
void fakeToPipe(struct vote_pipe* pipe, int n) {
  pipe->buff_index = (pipe->buff_index + n) % MAX_VOTE_PIPE_BUFF;
  pipe->buff_count = pipe->buff_count - n;  
}

// checks if n bytes match (wrappes memcmp, so same returns... kinda)
int compareBuffs(struct vote_pipe *pipeA, struct vote_pipe *pipeB, int n) {
  if (pipeA->buff_index != pipeB->buff_index) {
    printf("Vote_buff compareBuffs: buffers do not have the same index!\n");
    return -1;
  }
  // have to deal with wrapping again!
  if (pipeA->buff_index + n < MAX_VOTE_PIPE_BUFF) {
    // simple case
    return memcmp(&(pipeA->buffer[pipeA->buff_index]), &(pipeB->buffer[pipeB->buff_index]), n);
  } else {
    int part_cmp = MAX_VOTE_PIPE_BUFF - pipeA->buff_index;
    int result = memcmp(&(pipeA->buffer[pipeA->buff_index]), &(pipeB->buffer[pipeB->buff_index]), part_cmp);
    if (result != 0) {
      return result;
    }
    return memcmp(&(pipeA->buffer[0]), &(pipeB->buffer[0]), n - part_cmp);
  }
}

void copyBuff(struct vote_pipe *dest_pipe, struct vote_pipe *src_pipe) {
  dest_pipe->buff_count = src_pipe->buff_count;
  dest_pipe->buff_index = src_pipe->buff_index;

  if (src_pipe->buff_index + src_pipe->buff_count < MAX_VOTE_PIPE_BUFF) {
    // simple case
    memcpy(&(dest_pipe->buffer[dest_pipe->buff_index]), &(src_pipe->buffer[src_pipe->buff_index]), src_pipe->buff_count);
  } else {
    int part_cpy = MAX_VOTE_PIPE_BUFF - src_pipe->buff_index;
    memcpy(&(dest_pipe->buffer[dest_pipe->buff_index]), &(src_pipe->buffer[src_pipe->buff_index]), part_cpy);
    memcpy(&(dest_pipe->buffer[0]), &(src_pipe->buffer[0]), src_pipe->buff_count - part_cpy);
  }
}

void printVoteBuff(struct vote_pipe *vp) {
  printf("VOTE PIPE:\n");
  printf("\tindex: %d\tcount: %d\nBuffer: ", vp->buff_index, vp->buff_count);
  int i;
  if (vp->buff_index + vp->buff_count < MAX_VOTE_PIPE_BUFF) {
    // simple case just print
    for (i = vp->buff_index; i < (vp->buff_index + vp->buff_count); i ++) {
      printf("%2x ", vp->buffer[i]);
    }
  } else {
    for (i = vp->buff_index; i < MAX_VOTE_PIPE_BUFF; i++) {
      printf("%2x ", vp->buffer[i]);
    }
    for (i = 0; i < (vp->buff_count - (MAX_VOTE_PIPE_BUFF - vp->buff_index)); i++) {
      printf("%2x ", vp->buffer[i]);
    }
  }
  printf("\n");
}
