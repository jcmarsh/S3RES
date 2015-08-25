#include "vote_buff.h"

void writeTest(struct vote_pipe *vp, int fd, char* str, int len) {
  if (write(fd, str, len) != len) {
    printf("Write fail\n");
  }
  if (pipeToBuff(vp) != 0) {
    printf("pipeToBuff error.\n");
  }
  // printVoteBuff(vp);
}

void readTest(struct vote_pipe *vp, int fds[2], int len, char *expected) {
  char buffer[100] = {0};
  if (buffToPipe(vp, fds[1], len) != 0) {
    printf("buffToPipe error.\n");
  }
  if (read(fds[0], buffer, len) != len) {
    printf("Read fail\n");
  }
  // printVoteBuff(vp);

  if (strcmp(buffer, expected) != 0) {
    printf("Read back data: %s\tDid not match expected: %s\n", buffer, expected);
  }
}

void setupPipe(struct vote_pipe *vp, int to_rep[], int from_rep[]) {
  vp->fd_in = 0;
  vp->fd_out = 0;
  vp->buff_count = 0;
  vp->buff_index = 0;

  int i;
  for (i = 0; i < MAX_VOTE_PIPE_BUFF; i++) {
    vp->buffer[i] = '#';
  }

  if (pipe(to_rep) != 0) {
    perror("Pipe Fail");
  }
  if (pipe(from_rep) != 0) {
    perror("Pipe Fail");
  }
}

// Needs to have MAX_VOTE_PIPE_BUFF set at somthing link 128 to test the edge cases)
int main (int argc, char ** argv) {
  struct vote_pipe pipeA;
  int to_repA[2];
  int from_repA[2];

  printf("Pipe character count: %d\n", MAX_VOTE_PIPE_BUFF);

  printf("\nTest read / write - ");

  setupPipe(&pipeA, to_repA, from_repA);
	
  pipeA.fd_in = to_repA[0];

  int i;
  for (i = 0; i < 2 * MAX_VOTE_PIPE_BUFF; i++) {
    writeTest(&pipeA, to_repA[1], "Seven C", 7);
    readTest(&pipeA, from_repA, 7, "Seven C");
  }
  printf(" - Test read / write over\n");

  printf("Test compare\n");
  struct vote_pipe pipeB;
  int to_repB[2];
  int from_repB[2];
  struct vote_pipe pipeC;
  int to_repC[2];
  int from_repC[2];

  setupPipe(&pipeB, to_repB, from_repB);
  setupPipe(&pipeC, to_repC, from_repC);

  pipeB.fd_in = to_repB[0];
  pipeC.fd_in = to_repC[0];

  writeTest(&pipeB, to_repB[1], "This is the same message", 24);
  writeTest(&pipeC, to_repC[1], "This is the same message", 24);

  printf("Compare buffers (==0): %d\n", compareBuffs(&pipeB, &pipeC, 24));

  writeTest(&pipeB, to_repB[1], "This is NOT same message", 24);
  writeTest(&pipeC, to_repC[1], "This is the same message", 24);

  printf("Compare buffers (==0): %d\n", compareBuffs(&pipeB, &pipeC, 24));
  printf("Compare buffers (!=0): %d\n", compareBuffs(&pipeB, &pipeC, 48));

  readTest(&pipeB, from_repB, 48, "will error");
  readTest(&pipeC, from_repC, 48, "will error");

  writeTest(&pipeB, to_repB[1], "This is a long message to see how things are working out. Well I hope.", 70);
  writeTest(&pipeC, to_repC[1], "This is a long message to see how things are working out. Well I hope.", 70);

  readTest(&pipeB, from_repB, 70, "will error");
  readTest(&pipeC, from_repC, 70, "will error");

  writeTest(&pipeB, to_repB[1], "This is the same message!", 25);
  writeTest(&pipeC, to_repC[1], "This is the same message", 24);

  printf("Compare buffers (==0): %d\n", compareBuffs(&pipeB, &pipeC, 24));

  readTest(&pipeB, from_repB, 25, "will error");
  readTest(&pipeC, from_repC, 24, "will error");

  writeTest(&pipeB, to_repB[1], "This is the same message!", 25);
  writeTest(&pipeC, to_repC[1], "This is the same message", 24);

  printf("Compare buffers (==-1): %d\n", compareBuffs(&pipeB, &pipeC, 24));

  printf("Test copy - ");
  struct vote_pipe pipeD;
  int to_repD[2];
  int from_repD[2];
  struct vote_pipe pipeE;
  int to_repE[2];
  int from_repE[2];

  setupPipe(&pipeD, to_repD, from_repD);
  setupPipe(&pipeE, to_repE, from_repE);
	
  pipeD.fd_in = to_repD[0];
  pipeE.fd_in = to_repE[0];

  for (i = 0; i < 2 * MAX_VOTE_PIPE_BUFF; i++) {
    writeTest(&pipeD, to_repD[1], "ABCDEFG", 7);
    copyPipe(&pipeE, &pipeD);

    if (compareBuffs(&pipeD, &pipeE, 7) != 0) {
      printf("Compare buffs failed\n");
    }

    readTest(&pipeD, from_repD, 7, "ABCDEFG");
    readTest(&pipeE, from_repE, 7, "ABCDEFG");
  }
  printf(" - Test copy over\n");

  return 0;
}
