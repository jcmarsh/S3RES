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

#define QUAD_PIN_POLICY -2 // replicas should be pinned to CPUS 1, 2, and 3

// Lemur Laptop
//#define CPU_MHZ 2301.000
#define CPU_MHZ 1200.000
//#define VOTER_PIN 0
//#define CONTROLLER_PIN QUAD_PIN_POLICY
#define VOTER_PIN 0
#define CONTROLLER_PIN 0

// Optiplex 990
//#define CPU_MHZ 3092.0
//#define VOTER_CPU 0 // bind voter to CPU 1
//#define CONTROLLER_PIN 0

// BeagleBone Black Rev C
//#define CPU_MHZ 1000.0
//#define VOTER_PIN 0
//#define CONTROLLER_PIN 0

#endif /* SYSTEM_H */
