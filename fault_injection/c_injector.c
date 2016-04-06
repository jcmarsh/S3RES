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

#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

//#include "taslimited.h"

#define SLEEP_USEC 5000 * 1000

void printUsage(void) {
  printf("Usage: ./c_injector [kill command] [controller_name_0 ... controller_name_n]\n");
  printf("\tSuggested kill commands: '9' for Exec (SIGKILL), '37' for SDCs, and '38' for CFE.\n");
  printf("If controllers are not specified, assumes: AStar ArtPot Filter Mapper\n");
}

// Expected that pids, weights, and names all have count elements allocated
int getPIDs(int count, int *pids, float *weights, char **names, const char *cmd) {
  FILE *pout;
  char line[80];
  int i = 0;

  pout = popen(cmd, "r");
  while (fgets(line, 80, pout) != NULL) {
    sscanf(line, "%d %f %s", &pids[i], &weights[i], names[i]);
    i++;
    if (i > count) {
      printf("Error, more components than expected.\n");
      return i;
    }
  }

  pclose(pout);

  return i;
}

int main(int argc, char *argv[]) {
  char *default_names[4] = {"AStar", "Filter", "Mapper", "ArtPot"};
  char **controller_names;
  int kill_cmd;
  unsigned int controller_count = 4;
  unsigned int process_count;
  int i, j;
  FILE *log_file;

  // InitTAS(0, 49); // Super high priority.

  if (argc < 2) {
    printUsage();
    exit(0);
  }

  kill_cmd = atoi(argv[1]);

  if (2 == argc) {
    // No process names specified, assume default 4
    controller_names = (char **)default_names;
  } else { // TODO: bug here
    controller_count = argc - 2;
    controller_names = (char **) malloc(sizeof(char*) * controller_count);
    for (i = 0; i < controller_count; i++) {
      controller_names[i] = argv[2+i];
    }
  }

  int *pids = (int *)malloc(controller_count * 3 * sizeof(int)); // 3 for TMR, assumed max
  float *weights = (float *)malloc(controller_count * 3 * sizeof(float)); // 3 for TMR, assumed max
  char **names = (char **)malloc(controller_count * 3 * sizeof(char *)); // 3 for TMR, assumed max
  for (i = 0; i < controller_count * 3; i++) {
    names[i] = (char *)malloc(sizeof(char *) * 25);
  }

  int name_length = 0;
  for (i = 0; i < controller_count; i++) {
    name_length += strlen(controller_names[i]) + 2; // 2 for the \| separator needed
  }
  name_length++;

  char * names_string = malloc(name_length * sizeof(char));
  int total_i = 0;
  for (i = 0; i < controller_count; i++) {
    for (j = 0; j < strlen(controller_names[i]); j++) {
      names_string[total_i++] = controller_names[i][j];
    }
    if (i + 1 < controller_count) {
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
  
  printf("c_injector kill signal: %d, usleep %d\n\tpid search: %s\n", kill_cmd, SLEEP_USEC, cmd_str);
  process_count = getPIDs(controller_count * 3, pids, weights, names, cmd_str); // # of process at startup.
  while(true) {
    int total = 0;
    usleep(SLEEP_USEC);

    total = getPIDs(process_count, pids, weights, names, cmd_str);
    if (total < process_count) {
      printf("Error, less processes found than named.\n");
    } else {
      // TODO: This program needs work.
      int kill_index = rand() % total;
      kill(pids[kill_index], kill_cmd);
      printf("Signal %d on %d (%s)\n", kill_cmd, pids[kill_index], names[kill_index]);
      fflush(stdout);
      /*
	float kill_index = (float)((rand() / (double)(RAND_MAX)) * 100);
	float psum = 0.0;

	for (i = 0; i < total; i++) {
	psum += weights[i];
	if (psum > kill_index) {
	fprintf(log_file, "Killing pid %d\n", pids[i]);
	kill(pids[i], kill_cmd);
	break;
	}
	}

	fprintf(log_file, "\tInjection done. %f %f\n", psum, kill_index);
      */
    }
  }
 
  return 0;
}
