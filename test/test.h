// Just some helpful bits for the test framework code.

#include "commtypes.h"
#include "replicas.h"
#include "system_config.h"

#define MAX_TYPED_PIPE_BUFF 4096 // This should be the limit kernel on pipes... or something reasonable.

struct replica rep;
struct vote_pipe pipes[PIPE_LIMIT];
