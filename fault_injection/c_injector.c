/*
 * Modeled off of injector.py for now at least.
 * Meant to by a faster alternative, need for resouce limited systems.
 * Print to text file instead of screen.
 *
 * Given a list of process names, will kill them at a rate of 2Hz,
 * based on a weighting from ps for now.
 *
 * James Marshall
 */

 /*
  * TODO: May need to support different signals (right now just kills)
  */

#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "taslimited.h"

void printUsage(void) {
	printf("Usage: ./c_injector <ignored> [controller_name_0 ... controller_name_n]\n");
	printf("\t'kill -9'\tsend SIGTERM is the current default\n");
	printf("\t'/bin/kill -s SIGRTMIN+2'\tSimulate Silent Data Corruption (NOT IMPLEMENTED)\n");
	printf("\t'/bin/kill -s SIGRTMIN+3'\tSimulate Control Flow Error (NOT IMPLEMENTED)\n");
	printf("If controllers are not specified, assumes: AStar ArtPot Filter Mapper\n");
}

int getPIDs(int *pids, float *weights, const char *cmd) {
	FILE *pout;
	char line[80];
	int i = 0;

	pout = popen(cmd, "r");
	while (fgets(line, 80, pout) != NULL) {
		sscanf(line, "%d %f", &pids[i], &weights[i]);
		i++;
	}

	pclose(pout);

	return i;
}

int main(int argc, char *argv[]) {
	char *default_names[4] = {"AStar", "Filter", "Mapper", "ArtPot"};
	char **process_names;
	unsigned int count = 4;
	int i, j;
	FILE *log_file;

	InitTAS(0, 97); // Super high priority.

	log_file = fopen("injector_log.txt", "w");
	if (log_file == NULL) {
		printf("ERROR: c_injector failed open file.\n");
		exit(0);
	}

	if (argc < 2) {
		printUsage();
		exit(0);
	}

	// Set as RT, high priority
	struct sched_param param;
	param.sched_priority = sched_get_priority_max(SCHED_RR) - 1;

	// set the scheduler as with policy of round robin (realtime)
	if (sched_setscheduler(getpid(), SCHED_RR, &param ) == -1) {
		printf("Running as non-RT\n");
	}

	if (2 == argc) {
		// No process names specified, assume default 4
		process_names = (char **)default_names;
	} else {
		process_names = &(argv[2]);
		count = argc - 2;
		printf("This is my test of args\n");
		for (i = 0; i < count; i++) {
			printf("\targ %d, %s\n", i, process_names[i]);
		}
	}

	int *pids = (int *)malloc(count * 3 * sizeof(int)); // 3 for TMR, assumed max
	float *weights = (float *)malloc(count * 3 * sizeof(float)); // 3 for TMR, assumed max

	int name_length = 0;
	for (i = 0; i < count; i++) {
		name_length += strlen(process_names[i]) + 2; // 2 for the \| separator needed, -1 for the \0
	}
	name_length--;

	char * names_string = malloc(name_length * sizeof(char));
	int total_i = 0;
	for (i = 0; i < count; i++) {
		for (j = 0; j < strlen(process_names[i]); j++) {
			names_string[total_i++] = process_names[i][j];
		}
		if (i + 1 < count) {
			names_string[total_i++] = '\\';
			names_string[total_i++] = '|';
		} else {
			names_string[total_i] = '\0';
		}
	}

	char* cmd_str;
	i = asprintf(&cmd_str, "ps -ao pid,pcpu,comm | grep \"%s\" | grep -v \"Test\" | grep -v \"defunct\"", names_string);
	free(names_string);

	srand(time(NULL));
	while(true) {
		int total = 0;
		usleep(500 * 1000);

		total = getPIDs(pids, weights, cmd_str);
		if (total < count) {
			printf("Error, less processes found than named.\n");
		} else {
			float kill_index = (float)((rand() / (double)(RAND_MAX)) * 100);
			float psum = 0.0;

			for (i = 0; i < total; i++) {
				psum += weights[i];
				if (psum > kill_index) {
					fprintf(log_file, "Killing pid %d\n", pids[i]);
					kill(pids[i], SIGKILL);
					break;
				}
			}
			
			fprintf(log_file, "\tInjection done. %f %f\n", psum, kill_index);
		}
	}

	return 0;
}
