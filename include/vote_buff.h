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

// Max number of bytes
#define MAX_PIPE_BUFF 4096
// Why 10? No reason.
#define PIPE_LIMIT 10

// Here more for convenience
typedef enum {NONE,	SMR, DMR, TMR, REP_TYPE_ERROR} replication_t;

static const char* REP_TYPE_T[] = {"NONE", "SMR", "DMR", "TMR", "REP_TYPE_ERROR"};

struct vote_pipe {
	// Only one of these will be set at a time
	int fd_in;
	int fd_out;

	// Only used by voter / replicas. Should be elsewhere.
	bool timed; // timers start on input timed pipe reads, reset on output timed pipe writes
	int buff_count;
	int buff_index;
	char buffer[MAX_PIPE_BUFF];
};

replication_t reptypeToEnum(char* type);

void resetVotePipe(struct vote_pipe* pipe);

// read n bytes


#endif // _VOTE_BUFF_H_