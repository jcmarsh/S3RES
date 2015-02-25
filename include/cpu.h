/*-----------------------------------------------------------------------------
cpu.h encapsulates all POSIX level cpu queries.

author: James Taylor : jrt@gwu.edu
-----------------------------------------------------------------------------*/

#ifndef _CPU_H_
#define _CPU_H_

//-----------------------------------------------------------------------------
#define _GNU_SOURCE

#include <sys/types.h>

//-----------------------------------------------------------------------------

#define DEFAULT_CPU    1 // By default has less interrupts (eth0 is set to 0)

//-----------------------------------------------------------------------------

/// cpu speed type aliasing cycle type.
typedef unsigned long long cpu_speed_t;
/// cpu identified type aliasing system type.
typedef int cpu_id_t;

//-----------------------------------------------------------------------------

/// The set of cpu operation errors.
typedef enum {
    CPU_ERROR_NONE,              ///< Operation completed successfully.
    CPU_ERROR_OPEN,              ///< A failure to open cpu info through the os.
    CPU_ERROR_READ,              ///< A failure to read cpu info from the os.
    CPU_ERROR_BIND,              ///< A failure to bind the process to the cpu.
    CPU_ERROR_NOEXIST            ///< A failure to find the requested cpu.
} cpu_error_e;

cpu_error_e cpu_bind( const pid_t pid, const cpu_id_t cpu );

//-----------------------------------------------------------------------------

#endif // _CPU_H_
