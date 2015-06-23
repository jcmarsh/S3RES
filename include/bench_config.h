//#define TIME_FULL_BENCH
//#define TIME_RESTART_SIGNAL
#define TIME_RESTART_REPLICA

// From http://stackoverflow.com/a/1644898
#define DEBUG_PRINT 1
#define debug_print(...) \
	do { if (DEBUG_PRINT) fprintf(stderr, ##__VA_ARGS__); } while (0)

#define DEBUG_MESSAGING
