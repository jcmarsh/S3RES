#include "../include/cpu.h"

#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/time.h"

//-----------------------------------------------------------------------------
/// Restrict the specified process to run only in the prescribed cpu. 
/// @param pid the process to bind.
/// @param cpu the cpu to bind to.
/// @return indicator of operation success of specified error.
cpu_error_e cpu_bind( const pid_t pid, const cpu_id_t cpu ) {
  cpu_set_t cpuset_mask;
  // zero out the cpu set
  CPU_ZERO( &cpuset_mask );
  // bind the processor with the zeroed mask 
  CPU_SET( cpu, &cpuset_mask );

  // set the affinity of the process to the mask containing the processor
  // this effectively binds the process to the processor
  if( sched_setaffinity( pid, sizeof(cpuset_mask), &cpuset_mask ) == -1 ) {
    // there was an error setting the affinity for the coordinator
    // NOTE: can check errno if this occurs
    return CPU_ERROR_BIND;
  }

//  // testing sanity check ... TO BE COMMENTED
//  int ret = sched_getaffinity( 0, sizeof(cpuset_mask), &cpuset_mask );
//  printf( " sched_getaffinity = %d, cpuset_mask = %08lx\n", sizeof(cpuset_mask), cpuset_mask );

  return CPU_ERROR_NONE;
}

//-----------------------------------------------------------------------------

