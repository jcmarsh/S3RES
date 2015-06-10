#include "inc/tas_time.h"

//-----------------------------------------------------------------------------
/// Wraps the rdtsc assembly call and macro into a C/C++ function.
/// @return the current timestamp as indicated by the rdtsc register.
timestamp_t generate_timestamp( void ) {
  timestamp_t ts;
  rdtscll( ts );
  return ts;
}
