/*-----------------------------------------------------------------------------
scheduler.h encapsulates all scheduling functionality for the system at both 
API level threads and POSIX level realtime scheduling.

author: James Taylor : jrt@gwu.edu
-----------------------------------------------------------------------------*/

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

//-----------------------------------------------------------------------------

#include "time.h"
#include "cpu.h"

//-----------------------------------------------------------------------------

/// The set of scheduling operation errors.
typedef enum {
  SCHED_ERROR_NONE = 0,          ///< Operation completed successfully.
  SCHED_ERROR_SIGNAL,            ///< An invalid signal was sent.
  SCHED_ERROR_PERMISSIONS,       ///< A process does not have required permissions.
  SCHED_ERROR_NOEXIST,           ///< The process specified does not exist.
  SCHED_ERROR_POLICY,            ///< A failure to set realtime policy.
  SCHED_ERROR_PRIORITY_QUERY,    ///< A failure when querying priority.
  SCHED_ERROR_PRIORITY_VALIDATE, ///< A failure to validate priority.
  SCHED_ERROR_PARAM_QUERY,       ///< A failure querying a scheduling parameter.
  SCHED_ERROR_GENERAL            ///< A general error in scheduling.
} sched_error_e;

// test
sched_error_e sched_set_realtime_policy( const pid_t pid, int *priority, const int offset ); // keep

//-----------------------------------------------------------------------------

#endif // _SCHEDULER_H_
