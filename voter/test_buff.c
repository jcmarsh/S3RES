#include "vote_buff.h"

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

void myReset(struct vote_pipe *vp) {
	vp->fd_in = 0;
	vp->fd_out = 0;
	vp->buff_count = 0;
	vp->buff_index = 0;
}

// Needs to have MAX_VOTE_PIPE_BUFF set at somthing link 128 to test the edge cases)
int main (int argc, char ** argv) {
	struct vote_pipe pipeA;
	int to_rep[2];
	int from_rep[2];

	myReset(&pipeA);

	printf("Some number: %d\n", MAX_VOTE_PIPE_BUFF);

	int i;
	for (i = 0; i < MAX_VOTE_PIPE_BUFF; i++) {
		pipeA.buffer[i] = '#';
	}

	if (pipe(to_rep) != 0) {
		perror("Pipe Fail");
		return -1;
	}
	if (pipe(from_rep) != 0) {
		perror("Pipe Fail");
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
	printf("Test compare\n");
	struct vote_pipe pipeB;
	int to_repB[2];
	int from_repB[2];
	struct vote_pipe pipeC;
	int to_repC[2];
	int from_repC[2];

	myReset(&pipeB);
	myReset(&pipeC);

	for (i = 0; i < MAX_VOTE_PIPE_BUFF; i++) {
		pipeB.buffer[i] = '#';
		pipeC.buffer[i] = '#';
	}

	if (pipe(to_repB) != 0) {
		perror("Pipe Fail");
		return -1;
	}
	if (pipe(to_repC) != 0) {
		perror("Pipe Fail");
		return -1;
	}
	if (pipe(from_repB) != 0) {
		perror("Pipe Fail");
		return -1;
	}
	if (pipe(from_repC) != 0) {
		perror("Pipe Fail");
		return -1;
	}

	pipeB.fd_in = to_repB[0];
	pipeC.fd_in = to_repC[0];

	writeTest(&pipeB, to_repB[1], "This is the same message", 24);
	writeTest(&pipeC, to_repC[1], "This is the same message", 24);

	printf("Compare buffers (==0): %d\n", compareBuffs(&pipeB, &pipeC, 24));

	writeTest(&pipeB, to_repB[1], "This is NOT same message", 24);
	writeTest(&pipeC, to_repC[1], "This is the same message", 24);

	printf("Compare buffers (==0): %d\n", compareBuffs(&pipeB, &pipeC, 24));
	printf("Compare buffers (!=0): %d\n", compareBuffs(&pipeB, &pipeC, 48));

	readTest(&pipeB, from_repB, 48);
	readTest(&pipeC, from_repC, 48);

	writeTest(&pipeB, to_repB[1], "This is a long message to see how things are working out. Well I hope.", 70);
	writeTest(&pipeC, to_repC[1], "This is a long message to see how things are working out. Well I hope.", 70);

	readTest(&pipeB, from_repB, 70);
	readTest(&pipeC, from_repC, 70);

	writeTest(&pipeB, to_repB[1], "This is the same message!", 25);
	writeTest(&pipeC, to_repC[1], "This is the same message", 24);

	printf("Compare buffers (==0): %d\n", compareBuffs(&pipeB, &pipeC, 24));

	readTest(&pipeB, from_repB, 25);
	readTest(&pipeC, from_repC, 24);

	writeTest(&pipeB, to_repB[1], "This is the same message!", 25);
	writeTest(&pipeC, to_repC[1], "This is the same message", 24);

	printf("Compare buffers (==-1): %d\n", compareBuffs(&pipeB, &pipeC, 24));

	printf("Test copy\n");
	struct vote_pipe pipeD;
	int to_repD[2];
	int from_repD[2];
	struct vote_pipe pipeE;
	int to_repE[2];
	int from_repE[2];

	myReset(&pipeD);
	myReset(&pipeE);

	for (i = 0; i < MAX_VOTE_PIPE_BUFF; i++) {
		pipeD.buffer[i] = '#';
		pipeE.buffer[i] = '#';
	}

	if (pipe(to_repD) != 0) {
		perror("Pipe Fail");
		return -1;
	}
	if (pipe(to_repE) != 0) {
		perror("Pipe Fail");
		return -1;
	}
	if (pipe(from_repD) != 0) {
		perror("Pipe Fail");
		return -1;
	}
	if (pipe(from_repE) != 0) {
		perror("Pipe Fail");
		return -1;
	}

	pipeD.fd_in = to_repD[0];
	pipeE.fd_in = to_repE[0];

	writeTest(&pipeD, to_repD[1], "This is the same message", 24);
	copyBuff(&pipeE, &pipeD);

	printf("Compare buffers (==0): %d\n", compareBuffs(&pipeD, &pipeE, 24));

	readTest(&pipeD, from_repD, 24);
	readTest(&pipeE, from_repE, 24);

	writeTest(&pipeE, to_repE[1], "This is a long message to see how things are working out. Well I hope.", 70);
	copyBuff(&pipeD, &pipeE);

	printf("Compare buffers (==0): %d\n", compareBuffs(&pipeD, &pipeE, 70));

	readTest(&pipeD, from_repD, 70);
	readTest(&pipeE, from_repE, 70);

	writeTest(&pipeD, to_repD[1], "This is a long message to see how things are working out. Well I hope.", 70);
	copyBuff(&pipeE, &pipeD);

	printf("Compare buffers (==0): %d\n", compareBuffs(&pipeD, &pipeE, 70));

	return 0;
}
