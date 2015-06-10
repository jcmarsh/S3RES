/*-----------------------------------------------------------------------------
time.h encapsulates all time operations including API types and queries and 
POSIX types and queries.

author: James Taylor : jrt@gwu.edu
-----------------------------------------------------------------------------*/

#ifndef _TIME_H_
#define _TIME_H_

//-----------------------------------------------------------------------------

#include <sys/time.h>           // POSIX time
#include "cpu.h"

//-----------------------------------------------------------------------------

/// cycle type in clock ticks aliasing rdtsc values dependent on cpu speed.
typedef cpu_speed_t cycle_t;

/// timestamp type in cycles.
typedef cycle_t timestamp_t;

//-----------------------------------------------------------------------------

/// Assembly call to get current content of rdtsc register
//#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#define rdtscll(value)				       \
  __asm__ ("rdtsc\n\t"				       \
	   "shl $(32), %%rdx\n\t"		       \
	   "or %%rax, %%rdx" : "=d" (value) : : "rax")


//-----------------------------------------------------------------------------

timestamp_t generate_timestamp( void );

#endif // _TIME_H_
