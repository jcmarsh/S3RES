// Test Generic Empty

#include "commtypes.h"
#include "replicas.h"
#include "system_config.h"
#include "taslimited.h"

const char* controller_name = "GenericEmpty";

int main(int argc, const char** argv) {
  printf("Usage: No voter -> GenericEmptyVoteTest\n");
  printf("       Voter -> GenericEmptyVoteTest <Voter_Name> <Redundancy_Level>\n");
  printf("       Voter -> GenericEmptyVoteTest <Voter_Name> <Redundancy_Level> <Controller_Name>\n");

  pid_t currentPID = 0;
  char** rep_argv;
  int pipe_in[2], pipe_out[2];

  InitTAS(VOTER_PIN, 50);

  pipe(pipe_in);
  pipe(pipe_out);

  if (argc == 1) {
    // No Voter
    rep_argv = malloc(sizeof(char *) * 6);
    rep_argv[0] = "GenericEmpty";
    rep_argv[1] = "80"; // priority
    rep_argv[2] = "2"; // Pipe Count (ignored)
    asprintf(&rep_argv[3], "%s:%d:%d", "MSG_BUFFER", pipe_in[0], 0);
    asprintf(&rep_argv[4], "%s:%d:%d", "MSG_BUFFER", 0, pipe_out[1]);
    rep_argv[5] = NULL;
  } else {
    if (argc == 4) {
      controller_name = argv[3];
    }
    // With Voter
    rep_argv = malloc(sizeof(char *) * 8);
    rep_argv[0] = argv[1]; // VoterM or Voterd
    rep_argv[1] = "GenericEmpty";
    rep_argv[2] = argv[2]; // SMR, DMR, or TMR
    rep_argv[3] = "800"; // Timeout
    rep_argv[4] = "80";  // priority
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

  sleep(2);

  timestamp_t last;
  while (1) {
    int send_buffer[] = {4, 0, 1, 2, 3};
    int receive_buffer[] = {0, 0, 0, 0, 0};

    last = generate_timestamp();
    write(pipe_in[1], send_buffer, sizeof(send_buffer));

    // read filtered data
    read(pipe_out[0], receive_buffer, sizeof(receive_buffer));

    timestamp_t current = generate_timestamp();

    printf("generic_empty_test_usec (%lf)\n", diff_time(current, last, CPU_MHZ));

    usleep(100000); // 10 Hz
  }
}
