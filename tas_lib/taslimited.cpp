#include "../include/taslimited.h"

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

void optOutRT() {
  pid_t pid;
  struct sched_param param;

  pid = getpid();
  param.sched_priority = 0;

  if( sched_setscheduler(pid, SCHED_OTHER, &param) == -1 ) {
    perror("taslimited: Failed setscheduler to non RT");
  }
}

int InitTAS(cpu_id_t cpu, cpu_speed_t *cpu_speed, int prio_offset) {
  pid_t pid;
  int priority; // TODO: This is only passed to scheduler call?

  pid = getpid();

  // R-Limit
  call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
  call_getrlimit(RLIMIT_RTPRIO);

  // Bind
  if( cpu_c::bind(pid, cpu) != cpu_c::ERROR_NONE ) {
    printf("InitTAS() failed calling cpu_c::_bind(pid, cpu), with pid %d, cpu %d\n", pid, cpu);
  }

  // Set Realtime Scheduling
  // set the process to be scheduled with realtime policy and max priority              
  //  priority = priority - 5; // running max priority is bad? testing. AHHH DOING THIS WRONG
  if( scheduler_c::set_realtime_policy( pid, priority, prio_offset + 5) != scheduler_c::ERROR_NONE ) {
    perror("(voter_b_driver) InitTAS() failed calling schedule_set_realtime_max(pid,priority)." );
  }

  // * get the cpu speed *
  if( cpu_c::get_speed( *cpu_speed, cpu ) != cpu_c::ERROR_NONE ) {
    printf("(voter_b_driver) InitTAS() failed calling cpu_c::get_frequency(cpu_speed,cpu)\n" );
  }
}

int EveryTAS() {  
  // lock current and future memory
  if (lockItUp() != 0) {
    printf("(voter_b_driver) InitTAS() failed calling lockItUp()\n" );
  }

  // Walk current memory to get it paged in
  if (forceMaps() != 0) {
    printf("(voter_b_driver) InitTAS() failed calling forceMaps()\n" );
  }
}
