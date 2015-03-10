#include "../include/taslimited.h"

#include <sys/resource.h>
#include <unistd.h>

#include "../include/scheduler.h"
#include "../include/force.h"

// From Gabe's cos_loader.c
void call_getrlimit(int id) {
  struct rlimit rl;

  if (getrlimit(id, &rl)) {
    perror("getrlimit: ");
  }
}

// From Gabe's cos_loader.c
void call_setrlimit(int id, rlim_t c, rlim_t m)
{
  struct rlimit rl;

  rl.rlim_cur = c;
  rl.rlim_max = m;
  if (setrlimit(id, &rl)) {
    perror("setrlimit: ");
  }
}

int InitTAS(cpu_id_t cpu, int prio_offset) {
  pid_t pid;
  int priority;
  int result;

  pid = getpid();

  // R-Limit
  call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
  call_getrlimit(RLIMIT_RTPRIO);

  // Bind
  if( cpu_bind(pid, cpu) != CPU_ERROR_NONE ) {
    printf("InitTAS() failed calling cpu_bind(pid, cpu), with pid %d, cpu %d\n", pid, cpu);
  }

  // Set Realtime Scheduling
  // set the process to be scheduled with realtime policy and max priority              
  result = sched_set_realtime_policy( pid, &priority, prio_offset);
  if( result != SCHED_ERROR_NONE ) {
    printf("InitTAS() failed calling schedule_set_realtime_policy(pid %d, priority %d, offset %d): %d\n", pid, priority, prio_offset + 5, result);
  }
}

int EveryTAS() {  
  // lock current and future memory
  if (lockItUp() != 0) {
    printf("(voter_d_driver) InitTAS() failed calling lockItUp()\n" );
  }

  // Walk current memory to get it paged in
  if (forceMaps() != 0) {
    printf("(voter_d_driver) InitTAS() failed calling forceMaps()\n" );
  }
}
