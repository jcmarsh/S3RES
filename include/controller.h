#include <signal.h>
#include <sys/select.h>

#include "bench_config.h"
#include "fd_client.h"

/* // SIGRTMIN is different in Dietlibc, so just hardcoding these for now.
#define TIMEOUT_SIGNAL SIGRTMIN + 0 // The voter's watchdog timer
#define RESTART_SIGNAL SIGRTMIN + 1 // Voter to replica signal to fork itself
#define SDC_SIM_SIGNAL SIGRTMIN + 2 // For inserting simulated SDCs
#define CFE_SIM_SIGNAL SIGRTMIN + 3 // For inserting simulated control flow errors
*/

#define TIMEOUT_SIGNAL 35 // The voter's watchdog timer
#define RESTART_SIGNAL 36 // Voter to replica signal to fork itself
#define SDC_SIM_SIGNAL 37 // For inserting simulated SDCs
#define CFE_SIM_SIGNAL 38 // For inserting simulated control flow errors


int initController(void);
