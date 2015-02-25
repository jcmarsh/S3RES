#include "../include/scheduler.h"

//#include "tcs.h"

#include <assert.h>
#include <sys/types.h>
#include <sched.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

//-----------------------------------------------------------------------------
/// A sanity check to validate whether an offset parameter exceeds the 
/// acceptable range of priority values for using a realtime schedule.
/// @param offset the priority offset to validate.
/// @return indicator of operation success or specified error.
sched_error_e sched_validate_realtime_priority_offset( const int offset ) {

  int max_priority, min_priority, max_offset;
  sched_error_e result;

  // get the max and if that errored, return that error
  max_priority = sched_get_priority_max( SCHED_RR );
  
  // get the min and if that errored, return that error
  min_priority = sched_get_priority_min( SCHED_RR ); 
  
  // compute the max offset
  max_offset = max_priority - min_priority;

  // if the offset greater than max offset, return invalid
  if( offset > max_offset ) return SCHED_ERROR_PRIORITY_VALIDATE;

  // success
  return SCHED_ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Sets the system schedule policy to realtime for a process with a priority 
/// relative to the maximum priority value.
/// @param pid a process identifier.
/// @param priority the priority that was assigned to the pid upon success.
/// @param offset the offset of priority relative to the max priority.
/// @return indicator of operation success or specified error.
sched_error_e sched_set_realtime_policy( const pid_t pid, int *priority, const int offset ) {
  struct sched_param param;
  int max_priority, expected_priority;
  sched_error_e result;

  // validate the priority offset
  result = sched_validate_realtime_priority_offset( offset );
  if( result != SCHED_ERROR_NONE ) return result;

  // get the maximum priority allowed by the scheduler
  max_priority = sched_get_priority_max( SCHED_RR );
  
  // compute the expected priority
  expected_priority = max_priority - offset;

  // set the scheduling parameter priority
  param.sched_priority = expected_priority;

  // set the scheduler as with policy of round robin (realtime)
  if( sched_setscheduler( pid, SCHED_RR, &param ) == -1 ) {
    if( errno == EINVAL ) return SCHED_ERROR_SIGNAL;
    else if( errno == EPERM ) return SCHED_ERROR_PERMISSIONS;
    else if( errno == ESRCH ) return SCHED_ERROR_NOEXIST;
    return SCHED_ERROR_GENERAL;
  }

  // validate the scheduling policy
  int policy = sched_getscheduler( pid );
  if( policy == -1 ) {
    if( errno == EINVAL ) return SCHED_ERROR_SIGNAL;
    else if( errno == EPERM ) return SCHED_ERROR_PERMISSIONS;
    else if( errno == ESRCH ) return SCHED_ERROR_NOEXIST;
    return SCHED_ERROR_GENERAL;
  }
  if( policy != SCHED_RR ) return SCHED_ERROR_POLICY;

  // query the priority again to check the value
  if( sched_getparam( pid, &param ) == -1 )
    return SCHED_ERROR_PARAM_QUERY;

  // update the actual priority
  // Note: on ERROR_PRIORITY_VALIDATE, can look at current priority
  *priority = param.sched_priority;

  // validate that the scheduler priority is equal to the max priority
  if( *priority != expected_priority ) 
    return SCHED_ERROR_PRIORITY_VALIDATE;

  // success
  return SCHED_ERROR_NONE;
}
