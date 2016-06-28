/*
 * Meant for configuration related to benchmarks
 */

#ifndef BENCH_H
#define BENCH_H

#define TIME_FULL_BENCH
//#define TEST_IPC_ROUND // Needs TIME_FULL_BENCH
//#define TIME_RESTART_SIGNAL // VoterD uses dietlibc, but replicas needs sigqueue (not supported)
//#define TIME_RESTART_REPLICA
//#define PIPE_SMASH // Not in VoterR right now.
//#define TIME_WAITPID

#ifdef PIPE_SMASH
  #define PIPE_FILL_SIZE 2048
#endif

#ifdef TEST_IPC_ROUND
  #define IPC_SIZE 4096
#endif // TEST_IPC_ROUND

//#define RUSAGE_ENABLE

// From http://stackoverflow.com/a/1644898
//   Had to change... always compiling is exactly what I don't want.
//#define DEBUG_PRINT

#ifdef DEBUG_PRINT
#include <stdio.h>
#define debug_print(...) \
	fprintf(stderr, ##__VA_ARGS__);
#else
#define debug_print(...) \
 	{}
#endif /* DEBUG_PRINT */

#endif /* BENCH_H */
