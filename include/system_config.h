/*
 * Meant for configuration related to system hardware
 */

#ifndef SYSTEM_H
#define SYSTEM_H

/*
 * The policy for pining to a specific CPU is a pain, and hardware specific.
 * VOTER_PIN: -1 for skipping the pin call, 0-n for the desired CPU (This will be for all Voters).
 * CONTROLLER_PIN: -1 for skipping the pin call, 0-n for the desired CPU (same for all controllers).
 *    OR -2 for the each rep to it's own policy?
 */

// Lemur
// CPU_MHZ is used for timing measurements. The laptop's i7 tsc accounts for cpu scaling, so even though I'm running at 1200 Mhz, CPU_MHZ should be 2301.
#define CPU_MHZ 2301.000

// Lemur Quad core, using all
#define QUAD_PIN_POLICY -2 // replicas should be pinned to CPUS 1, 2, and 3
#define QUAD_CORE
#define VOTER_PIN 0
#define CONTROLLER_PIN QUAD_PIN_POLICY

// Lemur using only single core
//#define QUAD_PIN_POLICY 0
//#define SINGLE_CORE
//#define VOTER_PIN 0
//#define CONTROLLER_PIN 0

// Optiplex 990
//#define CPU_MHZ 3092.0
//#define VOTER_CPU 0 // bind voter to CPU 1
//#define CONTROLLER_PIN 0

// BeagleBone Black Rev C
//#define SINGLE_CORE
//#define CPU_MHZ 1000.0
//#define VOTER_PIN 0
//#define CONTROLLER_PIN 0

#endif /* SYSTEM_H */
