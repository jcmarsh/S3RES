#include <signal.h>

#include "../include/taslimited.h"
#include "../include/commtypes.h"
#include "../include/fd_client.h"

int initReplica(void);
static void restartHandler(int signo, siginfo_t *si, void *unused);
//void restartHandler(int signo);