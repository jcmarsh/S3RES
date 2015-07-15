#include "vote_buff.h"

printVoteBuff(struct vote_pipe *vp) {
	printf("VOTE PIPE:\n");
	printf("\tindex: %d\tcount: %d\n", vp->buff_index, vp->buff_count);
}

writeTest(struct vote_pipe *vp, int fd, char* str, int len) {
	if (write(fd, str, len) != len) {
		printf("Write fail\n");
		return -1;
	}
	if (pipeToBuff(vp) != 0) {
		printf("pipeToBuff error.\n");
	}
	printVoteBuff(vp);
}

readTest(struct vote_pipe *vp, int fds[2], int len) {
	char buffer[100];
	if (buffToPipe(vp, fds[1], len) != 0) {
		printf("buffToPipe error.\n");
	}
	if (read(fds[0], buffer, len) != len) {
		printf("Read fail\n");
	}
	printVoteBuff(vp);
	printf("Read back data: %s\n", buffer);
}

int main (int argc, char ** argv) {
	struct vote_pipe pipeA;
	int to_rep[2];
	int from_rep[2];

	if (pipe(to_rep) != 0) {
		printf("Pipe Fail");
		return -1;
	}
	if (pipe(from_rep) != 0) {
		printf("Pipe Fail");
		return -1;
	}

	pipeA.fd_in = to_rep[0];

	printf("Testing pipeToBuff\n");
	printVoteBuff(&pipeA);

	
	writeTest(&pipeA, to_rep[1], "Happy", 6);
	writeTest(&pipeA, to_rep[1], "Sad", 4);

	readTest(&pipeA, from_rep, 6);

	writeTest(&pipeA, to_rep[1], "Working?", 9);

	readTest(&pipeA, from_rep, 4);
	readTest(&pipeA, from_rep, 9);

	int loop;
	for (loop = 0; loop < 96; loop++) {
		printf("Loop: %d\n", loop);
		writeTest(&pipeA, to_rep[1], "A very long message to help test the behavior after wrapping.", 62);
		readTest(&pipeA, from_rep, 62);
	}

	return 0;
}