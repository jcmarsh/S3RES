#include "commtypes.h"

#include <signal.h>

#include "bench_config.h"
#include "taslimited.h"
#include "fd_client.h"

#define TIMEOUT_SIGNAL SIGRTMIN + 0 // The voter's watchdog timer
#define RESTART_SIGNAL SIGRTMIN + 1 // Voter to replica signal to fork itself
#define SDC_SIM_SIGNAL SIGRTMIN + 2 // For inserting simulated SDCs
#define CFE_SIM_SIGNAL SIGRTMIN + 3 // For inserting simulated control flow errors

int initController(void);
static void restartHandler(int signo, siginfo_t *si, void *unused);
