#include "vote_buff.h"

printVoteBuff(struct vote_pipe *vp) {
	printf("VOTE PIPE:\n");
	printf("\tindex: %d\tcount: %d\n", vp->buff_index, vp->buff_count);
	int i;
	for (i = 0; i < MAX_VOTE_PIPE_BUFF; i++) {
		printf("%c", vp->buffer[i]);
	}
	printf("\n");
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
	char buffer[100] = {0};
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

	resetVotePipe(&pipeA);

	int i;
	for (i = 0; i < MAX_VOTE_PIPE_BUFF; i++) {
		pipeA.buffer[i] = '#';
	}

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

	writeTest(&pipeA, to_rep[1], "Happy", 5);
	writeTest(&pipeA, to_rep[1], "Sad", 3);

	readTest(&pipeA, from_rep, 5);

	writeTest(&pipeA, to_rep[1], "Working?", 8);

	readTest(&pipeA, from_rep, 3);
	readTest(&pipeA, from_rep, 8);

	writeTest(&pipeA, to_rep[1], "This is a long message to see how things are working out. Well I hope.", 70);
	readTest(&pipeA, from_rep, 70);

	printf("Write another long one.\n");
	writeTest(&pipeA, to_rep[1], "This is a long message to see abcdefghijklmnopqrstuv out. Well I hope.", 70);

	printf("Write Shorty.\n");
	writeTest(&pipeA, to_rep[1], "Shorty.", 7);

	readTest(&pipeA, from_rep, 70);
	readTest(&pipeA, from_rep, 7);

	writeTest(&pipeA, to_rep[1], "This is a long message to see how things are working out. Well I hope.", 70);
	readTest(&pipeA, from_rep, 70);

	// test edge... index is 105
	writeTest(&pipeA, to_rep[1], "I need to write 24chars.", 24);
	readTest(&pipeA, from_rep, 24);	

	// TODO: Test the compare function.

	return 0;
}
