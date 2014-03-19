#include "replicas.h"
#include "register_util.h"
#include <wait.h>

int setupSignal(int signal_ignored);

int handleProcess(struct replica_group* rg, pid_t pid, int status, int insert_error);

void printResultsULong(struct replica* replicas, int child_num,  unsigned long* results);

void printResultsDoubles(struct replica* replicas, int child_num, double[][2]);
