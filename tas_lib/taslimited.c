#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#ifndef NO_PTHREAD
#include <pthread.h>
#endif /* NO_PTHREAD */

#include "inc/taslimited.h"
#include "inc/force.h"

int sched_set_policy(const pid_t pid, const int priority) {
  struct sched_param param;

  // get the maximum priority allowed by the scheduler
  if (priority >= sched_get_priority_max(SCHED_RR)) { // <= so that the priority can not be 99 (reserved for kernel)
    // printf("Invalid parameter for priority: %d\n", priority);
    perror("sched_get_priority_max ");
    return -1;
  }


  if (priority > 0) {
    param.sched_priority = priority;

    // set the scheduler as with policy of round robin (realtime)
    #ifdef NO_PTHREAD
    // The voters use dietlibc which implements sched_set.
    // Components use musl, which does not.
    // pthread requires about 13k to link, thus voters avoid it.
    if( sched_setscheduler( pid, SCHED_RR, &param ) == -1 ) {
      // This happens if the process is killed, but VoterD hasn't noticed yet and tries to balance the reps.
      // perror("sched_setscheduler SCHED_RR ");
      return -1;
    }
    #else
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &param) < 0) {
      puts("pthread_setschedparam error\n");
      return -1;
    }
    #endif /* NO_PTHREAD */
  } else {
    param.sched_priority = 0;

    #ifdef NO_PTHREAD
    if (sched_setscheduler(pid, SCHED_OTHER, &param) == -1 ) {
      perror("sched_setscheduler SCHED_OTHER ");
      return -1;
    }
    #else
    if (pthread_setschedparam(pthread_self(), SCHED_OTHER, &param) < 0) {
      puts("pthread_setschedparam error\n");
      return -1;
    }
    #endif /* NO_PTHREAD */

    // Set niceness
    int nice = priority + (priority * -2) - 20;
    if (setpriority(PRIO_PROCESS, pid, nice) < 0) {
      // perror("setpriority");
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

int RefreshTAS(void) {
  if (lockItUp() != 0) {
    puts("(voter_d_driver) InitTAS() failed calling lockItUp()\n" );
  }

  // Walk current memory to get it paged in
  if (forceMaps() != 0) {
    puts("(voter_d_driver) InitTAS() failed calling forceMaps()\n" );
  }

  return 0;
}

int InitTAS(cpu_id_t cpu, int priority) {
  pid_t pid;
  int result;

  pid = getpid();

  // R-Limit
  call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
  call_getrlimit(RLIMIT_RTPRIO);

  // Bind
  if (cpu < 0) {
    // Do not bind, this is for the single core case
  } else {
    if( cpu_bind(pid, cpu) != CPU_ERROR_NONE ) {
      perror("cpu_bind failed ");
    }
  }

  // Set Scheduling
  if( sched_set_policy(pid, priority) != 0 ) {
    perror("\tperror");
  }

  return RefreshTAS();
}
