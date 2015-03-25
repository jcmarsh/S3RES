#include "../include/taslimited.h"

#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#include "../include/force.h"

int sched_set_policy(const pid_t pid, const int priority) {
  struct sched_param param;

  // get the maximum priority allowed by the scheduler
  if (priority >= sched_get_priority_max(SCHED_RR)) { // <= so that the priority can not be 99 (reserved for kernel)
    printf("Invalid parameter for priority: %d\n", priority);
    return -1;
  }

  if (priority > 0) {
    param.sched_priority = priority;

    // set the scheduler as with policy of round robin (realtime)
    if( sched_setscheduler( pid, SCHED_RR, &param ) == -1 ) {
      return -1;
    }
  } else {
    param.sched_priority = 0;

    if (sched_setscheduler(pid, SCHED_OTHER, &param) == -1 ) {
      return -1;
    }

    // Set niceness
    int nice = priority + (priority * -2) - 20;
    if (setpriority(PRIO_PROCESS, pid, nice) < 0) {
      return -1;
    }
  }

  return 0;
}

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

int InitTAS(cpu_id_t cpu, int priority) {
  pid_t pid;
  int result;

  pid = getpid();

  // R-Limit
  call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
  call_getrlimit(RLIMIT_RTPRIO);

  // Bind
  if( cpu_bind(pid, cpu) != CPU_ERROR_NONE ) {
    printf("InitTAS() failed calling cpu_bind(pid, cpu), with pid %d, cpu %d\n", pid, cpu);
  }

  // Set Scheduling
  if( sched_set_policy(pid, priority) != 0 ) {
    printf("InitTAS() failed calling schedule_set_policy(pid %d, priority %d)\n", pid, priority);
    perror("\tperror");
  }

  return 0;
}

void EveryTAS(void) {  
  // lock current and future memory
  //timestamp_t last = generate_timestamp();
  if (lockItUp() != 0) {
    printf("(voter_d_driver) InitTAS() failed calling lockItUp()\n" );
  }
  //printf("lockItUp: (%lld)\n", generate_timestamp() - last);

  // Walk current memory to get it paged in
  if (forceMaps() != 0) {
    printf("(voter_d_driver) InitTAS() failed calling forceMaps()\n" );
  }
}
