

// Timing code, do not define to remove
// All done by ifdefs for now
//#define TIME_MAIN_LOOP
//#define TIME_ART_POT
#define TIME_FORK
//#define TIME_LASER_UPDATE
#define TIME_VOTE_CYCLE

// Not implemented
#define TIME_ROUND_TRIP
#define TIME_PING_PONG

#define N_IN_S 1000000000 // nano seconds in a second
#define N_IN_U 1000.0 // nano seconds in a micro, must be floating

#define PRINT_MICRO(M, S, E) printf("%s: %lf us\n", M, (((E.tv_sec - S.tv_sec) * N_IN_S) + (E.tv_nsec - S.tv_nsec)) / N_IN_U);

#define PRINT_SINGLE(M, T) printf("%s: \t%ld s\t%lf us\n", M, T.tv_sec, (T.tv_nsec / N_IN_U));
