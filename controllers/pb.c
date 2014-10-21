/*
 * Set up all of the various components in the system
 *
 * James Marshall
 */

#include <stdio.h>
#include <stdlib.h>

// These fds are from the benchmarker.
int read_in_fd;
int write_out_fd;

// Execution is kicked off by the benchmarker
// Bench has one read in fd and one write out.
// If you want to split it up, go for it.
int main (int argc, char** argv) {
	if (argc < 3) {
		printf("PB: No fds supplied; bailing\n");
		return -1;
	}

	read_in_fd = atoi(argv[1]);
	write_out_fd = atoi(argv[2]);

	char first[20];
	char opp[3];
	char second[20];

	const char file_name[] = "run_pb.cfg";
	const char file_mode[] = "r";
	// Read a configuration file to know what components to spawn
	FILE *fp = fopen(file_name, file_mode);
	
	fscanf(fp, "%s", first, 20);
	fscanf(fp, "%s", opp, 3);
	fscanf(fp, "%s", second, 20);

	printf("Whoa: %s|%s|%s\n", first, opp, second);


	// Each component has one read_in, but can write out to multiple queues
	// This means that the component has to sort its own messages, and know where to send them.
	// can be made more general later.

	// start each component, remember those already started
	// deal with circular dependencies


	return fclose(fp);
}