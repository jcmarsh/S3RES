#include "../include/taslimited.h"

// From Gabe's cos_loader.c
void call_getrlimit(int id, char *name) {
  struct rlimit rl;

  if (getrlimit(id, &rl)) {
    perror("getrlimit: ");
  }
    //  printf("rlimit for %s is %d:%d (inf %d)\n", 
    //  	 name, (int)rl.rlim_cur, (int)rl.rlim_max, (int)RLIM_INFINITY);
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

int InitTAS(cpu_id_t cpu, cpu_speed_t *cpu_speed) {
  pid_t pid;
  int priority;

  pid = getpid();

  // R-Limit
  //call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
  call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
  call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");

  // Bind
  if( cpu_c::bind(pid, cpu) != cpu_c::ERROR_NONE ) {
    printf("(voter_b_driver) InitTAS() failed calling cpu_c::_bind(pid,DEFAULT_CPU).\n");
  }

  // Set Realtime Scheduling
  // set the process to be scheduled with realtime policy and max priority              
  if( scheduler_c::set_realtime_policy( pid, priority ) != scheduler_c::ERROR_NONE ) {
    printf("(voter_b_driver) InitTAS() failed calling schedule_set_realtime_max(pid,priority).\n" );
  }
  //  printf( "process priority: %d\n", priority );


  // * get the cpu speed *
  if( cpu_c::get_speed( *cpu_speed, cpu ) != cpu_c::ERROR_NONE ) {
    printf("(voter_b_driver) InitTAS() failed calling cpu_c::get_frequency(cpu_speed,cpu)\n" );
  }
  //  printf("CPU Speed: %lld\n", *cpu_speed);

  // lock current and future memory
  if (lockItUp() != 0) {
    printf("(voter_b_driver) InitTAS() failed calling lockItUp()\n" );
  }

  // Walk current memory to get it paged in
  if (forceMaps() != 0) {
    printf("(voter_b_driver) InitTAS() failed calling forceMaps()\n" );
  }
}
