#include "../../include/scheduler.h"

//#include "tcs.h"

#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

//#include "thread.h"
//#include "osthread.h"
//#include "notification.h"
//#include "log.h"

#include <stdio.h>             // remove after all debugging printf's removed

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/// Forks the main process and executes the specified program in the newly
/// spawned child process after binding it to a cpu and scheduling it with a 
/// realtime policy.
/// @param pid the process identifier assigned to the newly forked process.
/// @param priority_offset the offset from max priority to assign the process.
/// @param processor the processor to bind the process to.
/// @param program the name of the program to execute.
/// @param as the name to give to the program.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::create( pid_t& pid, const int& priority_offset, const cpu_id_t& cpu, const char* program, const char* as ) {
// TODO: adjust sig to use the args as we will use
// TODO: per discussion, any args should be written to a file or stdin then
//       passed into the new processes either by filename or dup stdin fd

  int priority;

  // fork the process
  pid = fork();

  // handle any fork error
  if( pid < 0 ) 
    return ERROR_CREATE;

  // if still inside the parent process, get out
  if( pid > 0 )
    return ERROR_NONE;

  // child process
  pid = getpid();

  // bind the child to the cpu  
  if( cpu_c::bind( pid, cpu ) != cpu_c::ERROR_NONE )
    _exit( 1 );     // kill the child process

  // set the scheduling policy and priority
  if( set_realtime_policy( pid, priority, priority_offset ) != scheduler_c::ERROR_NONE )
    _exit( 1 );     // kill the child process

  // TODO: adjust the args execute the external program using either
  // execve( program, ?, ? );
  execl( program, as, NULL );

  // kill the child process
  _exit( 1 );

  // just a formality to eliminate the compiler warning
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// ** Test **
/// Forks the main process and executes the specified program in the newly
/// spawned child process after binding it to a cpu and scheduling it with a 
/// realtime policy.
/// @param pid the process identifier assigned to the newly forked process.
/// @param priority_offset the offset from max priority to assign the process.
/// @param processor the processor to bind the process to.
/// @param program the name of the program to execute.
/// @param as the name to give to the program.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::create( pid_t& pid, const int& priority_offset, const cpu_id_t& cpu, worker_f worker ) {
// TODO: adjust sig to use the args as we will use
// TODO: per discussion, any args should be written to a file or stdin then
//       passed into the new processes either by filename or dup stdin fd

  int priority;

  // fork the process
  pid = fork();

  // handle any fork error
  if( pid < 0 ) 
    return ERROR_CREATE;

  // if still inside the parent process, get out
  if( pid > 0 )
    return ERROR_NONE;

  // child process
  pid = getpid();

  // bind the child to the cpu  
  if( cpu_c::bind( pid, cpu ) != cpu_c::ERROR_NONE )
    _exit( 1 );     // kill the child process

  // set the scheduling policy and priority
  if( set_realtime_policy( pid, priority, priority_offset ) != scheduler_c::ERROR_NONE )
    _exit( 1 );     // kill the child process

  // call the worker
  void* arg;
  worker( arg );

  // kill the child process
  _exit( 1 );

  // just a formality to eliminate the compiler warning
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// API function to send a suspend signal to a child process.  
/// Suspend functionality in the API is actually a function of priority and not
/// the result of an explicit signal, so this function is provided but not
/// necessarily used.
/// @param pid process identifier.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::suspend( const pid_t& pid ) {
  assert( KILL_ALLOWED );

  int result = kill( pid, SIGSTOP );
  if( result == -1 ) {
    if( errno == EINVAL ) return ERROR_SIGNAL;
    else if( errno == EPERM ) return ERROR_PERMISSIONS;
    else if( errno == ESRCH ) return ERROR_NOEXIST;
    return ERROR_GENERAL;
  }
  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// API function to send a resume signal to a child process.
/// Resume functionality in the API is actually a function of priority and not
/// the result of an explicit signal, so this function is provided but not
/// necessarily used.
/// @param pid process identifier.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::resume( const pid_t& pid ) {
  assert( KILL_ALLOWED );

  int result = kill( pid, SIGCONT );
  if( result == -1 ) {
    if( errno == EINVAL ) return ERROR_SIGNAL;
    else if( errno == EPERM ) return ERROR_PERMISSIONS;
    else if( errno == ESRCH ) return ERROR_NOEXIST;
    return ERROR_GENERAL;
  }
  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// API function to send a terminate signal to a child process.
/// @param pid process identifier.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::terminate( const pid_t& pid ) {
  assert( KILL_ALLOWED );

  int result = kill( pid, SIGTERM );
  if( result == -1 ) {
    if( errno == EINVAL ) return ERROR_SIGNAL;
    else if( errno == EPERM ) return ERROR_PERMISSIONS;
    else if( errno == ESRCH ) return ERROR_NOEXIST;
    return ERROR_GENERAL;
  }
  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Sets the system schedule policy to realtime for a process with maximum 
/// priority.
/// @param pid a process identifier.
/// @param priority the priority that was assigned to the pid upon success.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::set_realtime_policy( const pid_t& pid, int& priority ) {

  struct sched_param param;
  int max_priority;

  // get the maximum priority allowed by the scheduler
  error_e result = get_realtime_max_priority( max_priority );
  if( result != ERROR_NONE )
    return result;

  // set the scheduling parameter priority
  param.sched_priority = max_priority;
/*
  call_getrlimit(RLIMIT_CPU, "CPU");
//#ifdef RLIMIT_RTTIME
  call_getrlimit(RLIMIT_RTTIME, "RTTIME");
//#endif
  call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
  call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
  call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");	
  call_getrlimit(RLIMIT_NICE, "NICE");
*/
  // set the scheduler as with policy of round robin (realtime)
  if( sched_setscheduler( pid, SCHED_RR, &param ) == -1 ) {
    if( errno == EINVAL ) return ERROR_SIGNAL;
    else if( errno == EPERM ) return ERROR_PERMISSIONS;
    else if( errno == ESRCH ) return ERROR_NOEXIST;
    return ERROR_GENERAL;
  }

  // validate the scheduling policy
  int policy = sched_getscheduler( pid );
  if( policy == -1 ) {
    if( errno == EINVAL ) return ERROR_SIGNAL;
    else if( errno == EPERM ) return ERROR_PERMISSIONS;
    else if( errno == ESRCH ) return ERROR_NOEXIST;
    return ERROR_GENERAL;
  }
  if( policy != SCHED_RR ) return ERROR_POLICY;

  // query the priority again to check the value
  if( sched_getparam( pid, &param ) == -1 )
    return ERROR_PARAM_QUERY;

  // update the actual priority
  // Note: on ERROR_PRIORITY_VALIDATE, can look at current priority
  priority = param.sched_priority;

  // validate that the scheduler priority is equal to the max priority
  if( priority != max_priority ) 
    return ERROR_PRIORITY_VALIDATE;

  // success
  return ERROR_NONE;
}
//-----------------------------------------------------------------------------
/// Sets the system schedule policy to realtime for a process with a priority 
/// relative to the maximum priority value.
/// @param pid a process identifier.
/// @param priority the priority that was assigned to the pid upon success.
/// @param offset the offset of priority relative to the max priority.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::set_realtime_policy( const pid_t& pid, int& priority, const int& offset ) {

  struct sched_param param;
  int max_priority, expected_priority;
  error_e result;

  // validate the priority offset
  result = validate_realtime_priority_offset( offset );
  if( result != ERROR_NONE ) return result;

  // get the maximum priority allowed by the scheduler
  result = get_realtime_max_priority( max_priority );
  if( result != ERROR_NONE )
    return result;

  // compute the expected priority
  expected_priority = max_priority - offset;

  // set the scheduling parameter priority
  param.sched_priority = expected_priority;
/*
  call_getrlimit(RLIMIT_CPU, "CPU");
//#ifdef RLIMIT_RTTIME
  call_getrlimit(RLIMIT_RTTIME, "RTTIME");
//#endif
  call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
  call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
  call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");	
  call_getrlimit(RLIMIT_NICE, "NICE");
*/
  // set the scheduler as with policy of round robin (realtime)
  if( sched_setscheduler( pid, SCHED_RR, &param ) == -1 ) {
    if( errno == EINVAL ) return ERROR_SIGNAL;
    else if( errno == EPERM ) return ERROR_PERMISSIONS;
    else if( errno == ESRCH ) return ERROR_NOEXIST;
    return ERROR_GENERAL;
  }

  // validate the scheduling policy
  int policy = sched_getscheduler( pid );
  if( policy == -1 ) {
    if( errno == EINVAL ) return ERROR_SIGNAL;
    else if( errno == EPERM ) return ERROR_PERMISSIONS;
    else if( errno == ESRCH ) return ERROR_NOEXIST;
    return ERROR_GENERAL;
  }
  if( policy != SCHED_RR ) return ERROR_POLICY;

  // query the priority again to check the value
  if( sched_getparam( pid, &param ) == -1 )
    return ERROR_PARAM_QUERY;

  // update the actual priority
  // Note: on ERROR_PRIORITY_VALIDATE, can look at current priority
  priority = param.sched_priority;

  // validate that the scheduler priority is equal to the max priority
  if( priority != expected_priority ) 
    return ERROR_PRIORITY_VALIDATE;

  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Gets the minimum system schedule realtime priority for a process.
/// @param priority the minimum system priority.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::get_realtime_min_priority( int& priority ) {

  // query the max priority
  priority = sched_get_priority_min( SCHED_RR );

  // on error
  // Note: must check return value, otherwise priority may be invalid
  if( priority == -1 ) return ERROR_PRIORITY_QUERY;

  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Gets the maximum system schedule realtime priority for a process.
/// @param priority the maximum system priority.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::get_realtime_max_priority( int& priority ) {

  // query the max priority
  priority = sched_get_priority_max( SCHED_RR );

  // on error
  // Note: must check return value, otherwise priority may be invalid
  if( priority == -1 ) return ERROR_PRIORITY_QUERY;

  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Gets the maximum system schedule realtime priority for a process.
/// @param priority the maximum system priority.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::get_realtime_relative_priority( int& priority, const int& offset ) {

  int max_priority;
  error_e result;

  // validate the priority offset
  result = validate_realtime_priority_offset( offset );
  if( result != ERROR_NONE ) return result;

  // get the max and if that errored, return that error
  result = get_realtime_max_priority( max_priority );
  if( result != ERROR_NONE ) return result;

  // compute the priority
  priority = max_priority - offset;

  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// A sanity check to validate whether an offset parameter exceeds the 
/// acceptable range of priority values for using a realtime schedule.
/// @param offset the priority offset to validate.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::validate_realtime_priority_offset( const int& offset ) {

  int max_priority, min_priority, max_offset;
  error_e result;

  // get the max and if that errored, return that error
  result = get_realtime_max_priority( max_priority );
  if( result != ERROR_NONE ) return result;

  // get the min and if that errored, return that error
  result = get_realtime_min_priority( min_priority );
  if( result != ERROR_NONE ) return result;

  // compute the max offset
  max_offset = max_priority - min_priority;

  // if the offset greater than max offset, return invalid
  if( offset > max_offset ) return ERROR_PRIORITY_VALIDATE;

  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Gets the system schedule priority for a process.
/// @param pid a process identifier.
/// @param priority the priority that is currently assigned to the process.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::get_priority( const pid_t& pid, int& priority ) {
  sched_param param;

  if( sched_getparam( pid, &param ) == -1 ) 
    return ERROR_PARAM_QUERY;

  priority = param.sched_priority;
  // success
  return ERROR_NONE;
}
//-----------------------------------------------------------------------------
/// Sets the system schedule priority for a process.
/// @param pid a process identifier.
/// @param priority the input requested priority and the output actual priority.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::set_priority( const pid_t& pid, int& priority ) {
  sched_param param;
  int requested_priority = priority;

  // get the scheduling parameter  
  if( sched_getparam( pid, &param ) == -1 )
    return ERROR_PARAM_QUERY;

  // if the scheduler priority is already set to the in priority, quasi success
  if( param.sched_priority == requested_priority ) 
    return ERROR_NONE;
/*
  call_getrlimit(RLIMIT_CPU, "CPU");
//#ifdef RLIMIT_RTTIME
  call_getrlimit(RLIMIT_RTTIME, "RTTIME");
//#endif
  call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
  call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
  call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");	
  call_getrlimit(RLIMIT_NICE, "NICE");
*/
  // set the scheduled priority to the requested priority
  param.sched_priority = requested_priority;
  if( sched_setparam( pid, &param ) == -1 )
    return ERROR_PARAM_UPDATE;

  // query the priority again to check the value
  if( sched_getparam( pid, &param ) == -1 )
    return ERROR_PARAM_QUERY;

  // update the actual priority
  // Note: on ERROR_PRIORITY_VALIDATE, can look at current priority
  priority = param.sched_priority;

  // validate that the scheduler priority is equal to the requested priority
  if( priority != requested_priority ) 
    return ERROR_PRIORITY_VALIDATE;
  
  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Decreases the system schedule realtime priority for a process by a 
/// specified amount.
/// @param pid a process identifier.
/// @param priority the priority that was assigned to the pid upon success.
/// @param offset the amount to decrease the currently assigned priority by.
/// @param min_priority the arbitrarily constrained floor of priorities which
/// must be greater than or equal to the system constrained floor of priorities.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::decrease_realtime_priority( const pid_t& pid, int& priority, const int& offset, const int& min_priority ) {
  struct sched_param param;
  int min_allowed;
  int target_priority;
  error_e result;
/*
  // validate the priority offset
  result = validate_realtime_priority_offset( offset );
  if( result != ERROR_NONE ) return result;

  // get the minimum priority allowed by the scheduler
  result = get_realtime_min_priority( min_allowed );
  if( result != ERROR_NONE )
    return result;

  // if min specified is not constrained to the scheduler min then fail
  if( min_priority < min_allowed ) 
    return ERROR_PRIORITY_BOUNDS;

  // get the scheduling parameter  
  if( sched_getparam( pid, &param ) == -1 )
    return ERROR_PARAM_QUERY;
*/
  //target_priority = param.sched_priority - offset;
  target_priority = priority - offset;
  // if the adjusted priority is less than the constrained min then fail
  if( target_priority < min_priority ) 
    return ERROR_PRIORITY_BOUNDS;

  // set the priority parameter
  param.sched_priority = target_priority;
  if( sched_setparam( pid, &param ) == -1 )
    return ERROR_PARAM_UPDATE;
///*  
  // query the priority again to check the value
  if( sched_getparam( pid, &param ) == -1 )
    return ERROR_PARAM_QUERY;
//*/
  // update the actual priority
  // Note: on ERROR_PRIORITY_VALIDATE, can look at current priority
  priority = param.sched_priority;
///*
  // validate that the scheduler priority is now equal to the requested change
  if( param.sched_priority != target_priority )
    return ERROR_PRIORITY_VALIDATE;
//*/
  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Increases the system schedule realtime priority for a process by a 
/// specified amount.
/// @param pid a process identifier.
/// @param priority the priority that was assigned to the pid upon success.
/// @param offset the amount to increase the currently assigned priority by.
/// @param max_priority the arbitrarily constrained ceiling of priorities which
/// must be less than or equal to the system constrained ceiling of priorities.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::increase_realtime_priority( const pid_t& pid, int& priority, const int& offset, const int& max_priority ) {
  struct sched_param param;
  int max_allowed;
  int target_priority;
  error_e result;
///*
  // validate the priority offset
  result = validate_realtime_priority_offset( offset );
  if( result != ERROR_NONE ) return result;

  // get the maximum priority allowed by the scheduler
  result = get_realtime_max_priority( max_allowed );
  if( result != ERROR_NONE )
    return result;

  // if max specified is not constrained to the scheduler max then fail
  if( max_priority > max_allowed ) 
    return ERROR_PRIORITY_BOUNDS;
//*/

  // get the scheduling parameter  
  if( sched_getparam( pid, &param ) == -1 )
    return ERROR_PARAM_QUERY;

  //printf( "pid:%d priority:%d offset:%d param:%d\n", pid, priority, offset, param.sched_priority );

  target_priority = param.sched_priority + offset;
  // if the adjusted priority exceeds the constrained max priority then fail
  if( target_priority > max_priority ) 
    return ERROR_PRIORITY_BOUNDS;

  //target_priority = priority + offset;

  // set the priority parameter
  param.sched_priority = target_priority;
  if( sched_setparam( pid, &param ) == -1 )
    return ERROR_PARAM_UPDATE;
///*  
  // query the priority again to check the value
  if( sched_getparam( pid, &param ) == -1 )
    return ERROR_PARAM_QUERY;
//*/
  // update the actual priority
  // Note: on ERROR_PRIORITY_VALIDATE, can look at current priority
  priority = param.sched_priority;
///*
  // validate that the scheduler priority is now equal to the requested change
  if( param.sched_priority != target_priority )
    return ERROR_PRIORITY_VALIDATE;
//*/
  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Creates a pthread and schedules it under a realtime policy.
/// @param thread the reference to the pthread that is created.
/// @param priority the desired priority of the pthread.
/// @param worker the function to be assigned to the thread.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::create( pthread_t& thread, const int& priority, worker_f worker ) {
  pthread_attr_t atts;
  struct sched_param param;
 
  // establish attributes
  if( pthread_attr_init( &atts ) != 0 )
    return ERROR_GENERAL; 

  // set an explicit schedule for the thread
  if( pthread_attr_setinheritsched( &atts, PTHREAD_EXPLICIT_SCHED )  != 0 ) 
    return ERROR_SCHEDULE; 

  // set schedule policy to round robin
  if( pthread_attr_setschedpolicy( &atts, SCHED_RR ) != 0 )
    return ERROR_POLICY;
 
  // set the priority
  param.sched_priority = priority;
  if( pthread_attr_setschedparam( &atts, &param ) != 0 )
    return ERROR_PARAM_UPDATE;

  // spawn the thread
  if( pthread_create( &thread, &atts, worker, NULL ) != 0 ) 
    return ERROR_CREATE;

  // clean up attributes
  if( pthread_attr_destroy( &atts ) != 0 )
    return ERROR_GENERAL; 

  // query the pthread's priority and policy
  int policy;
  if( pthread_getschedparam( thread, &policy, &param ) != 0 )
    return ERROR_PARAM_QUERY;

  // validate the policy
  if( policy != SCHED_RR )
    return ERROR_POLICY;

  // validate the priority
  if( param.sched_priority != priority )
    return ERROR_PRIORITY_VALIDATE;

  //priority = param.sched_priority;

  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Gets the system schedule priority for a pthread.
/// @param thread the pthread.
/// @param priority the priority that is currently assigned to the process.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::get_priority( const pthread_t& thread, int& priority ) {

  sched_param param;
  int policy;

  int result = pthread_getschedparam( thread, &policy, &param );
  if( result != 0 )
    return ERROR_PARAM_QUERY;

  priority = param.sched_priority;
  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Sets the system schedule priority for a pthread.
/// @param thread the pthread.
/// @param priority the input requested priority and the output actual priority.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::set_priority( const pthread_t& thread, int& priority ) {

  sched_param param;
  int policy;
  int requested_priority = priority;
  int result;

  // get the scheduling parameter    
  result = pthread_getschedparam( thread, &policy, &param );
  if( result != 0 )
    return ERROR_PARAM_QUERY;

  // if the scheduler priority is already set to the in priority, quasi success
  if( param.sched_priority == requested_priority )
    return ERROR_NONE;

  // set the scheduled priority to the requested priority
  param.sched_priority = requested_priority;
  result = pthread_setschedparam( thread, SCHED_RR, &param ); 
  if( result != 0 )
    return ERROR_PARAM_UPDATE;

  // query the priority again to check the value
  result = pthread_getschedparam( thread, &policy, &param );
  if( result != 0 )
    return ERROR_PARAM_QUERY;

  // update the actual priority
  // Note: on ERROR_PRIORITY_VALIDATE, can look at current priority
  priority = param.sched_priority;

  // validate that the scheduler priority is equal to the requested priority
  if( priority != requested_priority ) 
    return ERROR_PRIORITY_VALIDATE;
 
  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Decreases the system schedule realtime priority for a pthread by a 
/// specified amount.
/// @param thread the pthread.
/// @param priority the priority that was assigned to the pid upon success.
/// @param offset the amount to decrease the currently assigned priority by.
/// @param min_priority the arbitrarily constrained floor of priorities which
/// must be greater than or equal to the system constrained floor of priorities.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::decrease_realtime_priority( const pthread_t& thread, int& priority, const int& offset, const int& min_priority ) {
  struct sched_param param;
  int min_allowed;
  int target_priority;
  error_e result;
  int presult;
  int policy;

  // SANITY CHECK that a negative number didn't creep in
  assert( offset > 0 );
/*
  // get the minimum priority allowed by the scheduler
  result = get_realtime_min_priority( min_allowed );
  if( result != ERROR_NONE )
    return result;

  // if min specified is not constrained to the scheduler min then fail
  if( min_priority < min_allowed ) 
    return ERROR_PRIORITY_BOUNDS;
*/
/*
  // get the scheduling parameter    
  presult = pthread_getschedparam( thread, &policy, &param );
  if( presult != 0 )
    return ERROR_PARAM_QUERY;
*/
  // Note: might be worthwhile to check the policy here

  //target_priority = param.sched_priority - offset;
  target_priority = priority - offset;
  // if the adjusted priority is less than the constrained min then fail
  if( target_priority < min_priority ) 
    return ERROR_PRIORITY_BOUNDS;

  // set the priority parameter
  param.sched_priority = target_priority;
  presult = pthread_setschedparam( thread, SCHED_RR, &param ); 
  if( presult != 0 )
    return ERROR_PARAM_UPDATE;
/*  
  // query the priority again to check the value
  presult = pthread_getschedparam( thread, &policy, &param );
  if( presult != 0 )
    return ERROR_PARAM_QUERY;
*/
  // Note: might be worthwhile to check the policy again here

  // update the actual priority
  // Note: on ERROR_PRIORITY_VALIDATE, can look at current priority
  priority = param.sched_priority;
/*
  // validate that the scheduler priority is now equal to the requested change
  if( param.sched_priority != target_priority )
    return ERROR_PRIORITY_VALIDATE;
*/
  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
/// Increases the system schedule realtime priority for a pthread by a 
/// specified amount.
/// @param thread the pthread.
/// @param priority the priority that was assigned to the pid upon success.
/// @param offset the amount to increase the currently assigned priority by.
/// @param max_priority the arbitrarily constrained ceiling of priorities which
/// must be less than or equal to the system constrained ceiling of priorities.
/// @return indicator of operation success or specified error.
scheduler_c::error_e scheduler_c::increase_realtime_priority( const pthread_t& thread, int& priority, const int& offset, const int& max_priority ) {
  struct sched_param param;
  int max_allowed;
  int target_priority;
  int presult;
  int policy;
  error_e result;
/*
  // validate the priority offset
  result = validate_realtime_priority_offset( offset );
  if( result != ERROR_NONE ) return result;

  // get the maximum priority allowed by the scheduler
  result = get_realtime_max_priority( max_allowed );
  if( result != ERROR_NONE )
    return result;

  // if max specified is not constrained to the scheduler max then fail
  if( max_priority > max_allowed ) 
    return ERROR_PRIORITY_BOUNDS;
*/
  // get the scheduling parameter  
  presult = pthread_getschedparam( thread, &policy, &param );
  if( presult != 0 )
    return ERROR_PARAM_QUERY;

  // Note: might be worthwhile to check the policy here

  target_priority = param.sched_priority + offset;
  // if the adjusted priority exceeds the constrained max priority then fail
  if( target_priority > max_priority ) 
    return ERROR_PRIORITY_BOUNDS;

  // set the priority parameter
  param.sched_priority = target_priority;
  presult = pthread_setschedparam( thread, SCHED_RR, &param ); 
  if( presult != 0 )
    return ERROR_PARAM_UPDATE;
/*  
  // query the priority again to check the value
  presult = pthread_getschedparam( thread, &policy, &param );
  if( presult != 0 )
    return ERROR_PARAM_QUERY;
*/
  // Note: might be worthwhile to check the policy again here

  // update the actual priority
  // Note: on ERROR_PRIORITY_VALIDATE, can look at current priority
  priority = param.sched_priority;
/*
  // validate that the scheduler priority is now equal to the requested change
  if( param.sched_priority != target_priority )
    return ERROR_PRIORITY_VALIDATE;
*/
  // success
  return ERROR_NONE;
}

//-----------------------------------------------------------------------------
