// Test Generic Empty

#include <unistd.h>

#include "commtypes.h"
#include "replicas.h"
#include "system_config.h"
#include "taslimited.h"

int main(int argc, char** argv) {
  printf("Usage: GenericEmptyVoteTest [-s send_size] [-v voter_name] [-c controller_name] [-r redundancy_level]\n");
  //  printf("       Voter -> GenericEmptyVoteTest <Voter_Name> <Redundancy_Level>\n");
  //  printf("       Voter -> GenericEmptyVoteTest <Voter_Name> <Redundancy_Level> <Controller_Name>\n");

  pid_t currentPID = 0;
  char** rep_argv;
  int pipe_in[2], pipe_out[2];
  int arg_ret;
  int send_size = 8;
  char* voter_name = NULL;
  char* redundancy_level = NULL;
  char* controller_name = "GenericEmpty";

  while ((arg_ret = getopt (argc, argv, "s:v:c:r:")) != -1) {
    switch(arg_ret) {
    case 's':
      send_size = atoi(optarg);
      break;
    case 'v':
      voter_name = optarg;
      break;
    case 'c':
      controller_name = optarg;
      break;
    case 'r':
      redundancy_level = optarg;
      break;
    case '?':
      fprintf (stderr, "Arg parsing error\n");
      return 1;
    default:
      abort ();
    }
  }

  InitTAS(VOTER_PIN, 50);

  pipe(pipe_in);
  pipe(pipe_out);

  if (NULL == voter_name) {
    // No Voter
    rep_argv = malloc(sizeof(char *) * 6);
    rep_argv[0] = controller_name;
    rep_argv[1] = "40"; // priority
    rep_argv[2] = "2"; // Pipe Count (ignored)
    asprintf(&rep_argv[3], "%s:%d:%d", "MSG_BUFFER", pipe_in[0], 0);
    asprintf(&rep_argv[4], "%s:%d:%d", "MSG_BUFFER", 0, pipe_out[1]);
    rep_argv[5] = NULL;
  } else {
    // With Voter
    rep_argv = malloc(sizeof(char *) * 8);
    rep_argv[0] = voter_name; // VoterM or Voterd
    rep_argv[1] = controller_name;
    rep_argv[2] = redundancy_level; // SMR, DMR, or TMR
    rep_argv[3] = "800"; // Timeout
    rep_argv[4] = "40";  // priority
    asprintf(&rep_argv[5], "%s:%d:%d:%d", "MSG_BUFFER", pipe_in[0], 0, 1);
    asprintf(&rep_argv[6], "%s:%d:%d:%d", "MSG_BUFFER", 0, pipe_out[1], 1);
    rep_argv[7] = NULL;
  }

  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      if (-1 == execv(rep_argv[0], rep_argv)) {
        printf("Exec ERROR!\n");
      }
    } else {
      InitTAS(VOTER_PIN, 50);
    }
  }

  printf("Running with send_size %d\n", send_size);

  sleep(2);

  timestamp_t last;
  while (1) {
    int write_ret, read_ret;
    char *send_buffer = malloc(sizeof(char) * send_size);
    char *receive_buffer = malloc(sizeof(char) * send_size);

    memset(send_buffer, 'a', send_size);

    last = generate_timestamp();
    write_ret = write(pipe_in[1], send_buffer, send_size);

    // read filtered data
    read_ret = read(pipe_out[0], receive_buffer, send_size);

    timestamp_t current = generate_timestamp();

    if (write_ret != send_size) {
      printf("ERROR: write sizes don't match %d - %d\n", write_ret, send_size);
    }

    if (read_ret != send_size) {
      printf("ERROR: read sizes don't match %d - %d\n", read_ret, send_size);
    }

    printf("generic_empty_test_usec (%lf)\n", diff_time(current, last, CPU_MHZ));

    usleep(100 * 1000); // 10 Hz
  }
}
