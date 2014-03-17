#include "replicas.h"
#include "register_util.h"
#include <wait.h>

int setupSignal(int signal_ignored);

int handleProcess(struct replica_group* rg, pid_t pid, int status, int insert_error);

void printResults(struct replica* replicas, int num);
