#include <signal.h>

#include "../include/taslimited.h"
#include "../include/commtypes.h"
#include "../include/statstime.h"
#include "../include/fd_client.h"

int initReplica(void);
void restartHandler(int signo);