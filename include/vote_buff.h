/*
 * James Marshall June 24, 2015
 */

#ifndef _VOTE_BUFF_H_
#define _VOTE_BUFF_H_

#define _GNU_SOURCE 1

#include <error.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Max number of bytes
#define MAX_VOTE_PIPE_BUFF 4096
// Why 10? No reason.
#define PIPE_LIMIT 10

// Here more for convenience
typedef enum {NONE, SMR, DMR, TMR, REP_TYPE_ERROR} replication_t;

static const char* REP_TYPE_T[] = {"NONE", "SMR", "DMR", "TMR", "REP_TYPE_ERROR"};

struct vote_pipe {
	char *rep_info; // extra string passed on to the replica. For example, the pipe's type
	
	// Only one of these will be set at a time
	int fd_in;
	int fd_out;

	// Only used by voter / replicas. Should be elsewhere.
	bool timed; // timers start on input timed pipe reads, reset on output timed pipe writes
	int buff_count;
	int buff_index;
	unsigned char buffer[MAX_VOTE_PIPE_BUFF];
};

replication_t reptypeToEnum(char* type);

void resetVotePipe(struct vote_pipe* pipe);

// read available bytes
int pipeToBuff(struct vote_pipe* pipe);

// write n bytes
int buffToPipe(struct vote_pipe* pipe, int fd_out, int n);
void fakeToPipe(struct vote_pipe* pipe, int n); // Advances buffer, but no need to write

int compareBuffs(struct vote_pipe *pipeA, struct vote_pipe *pipeB, int n);
void copyBuff(struct vote_pipe *dest_pipe, struct vote_pipe *src_pipe);

void printVoteBuff(struct vote_pipe *vp);
#endif // _VOTE_BUFF_H_
