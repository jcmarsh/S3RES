/*
 * Just a few defines to make taking timing stats easier
 *
 * James Marshall
 */

#ifndef _STATS_TIME_H_
#define _STATS_TIME_H_

// How long it takes the benchmarker to send range data and recieve a command back
#define _STATS_BENCH_ROUND_TRIP_

// Time spent inside of Art_Pot_P, read in range, send command
//#define _STATS_CONT_COMMAND_
// Time from the benchmarker to the controller
//#define _STATS_BENCH_TO_CONT_
// Time from the controller to the benchmarker
//#define _STATS_CONT_TO_BENCH_

#endif // _STATS_TIME_H
