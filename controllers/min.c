/*
 * Author - James Marshall
 */

#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/replicas.h"
#include "../include/commtypes.h"

#define REP_COUNT 2

// Replica related data
struct replica replicas[REP_COUNT];

char* controller_name;
// pipes to external components (not replicas)
int pipe_count = 0;
struct typed_pipe ext_pipes[PIPE_LIMIT];

int main(int argc, const char **argv) {
  controller_name = const_cast<char*>(argv[1]);

  initReplicas(replicas, REP_COUNT, controller_name, 10);
  createPipes(replicas, REP_COUNT, NULL, 0);
  forkReplicas(replicas, REP_COUNT);

  while(1) {
    sleep(1);
  }

  return 0;
}