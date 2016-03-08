// Test GenericEmpty

#include <time.h>
#include "test.h"

#include "taslimited.h"

const char* controller_name = "GenericEmpty";

int main(int argc, const char** argv) {
  printf("Usage: No voter -> GEVoteTest\n");
  printf("       Voter -> GEVoteTest <Voter_Name> <Redundancy_Level>\n");
  
  InitTAS(VOTER_PIN, 50); // run on CPU 0 (reps should either be spread out on 1,2,3 or all on 0)

  pid_t currentPID = 0;
  char** rep_argv;
  int pipe_in[2], pipe_out[2];

  pipe(pipe_in);
  pipe(pipe_out);

  if (argc == 1) {
    // No Voter
    rep_argv = malloc(sizeof(char *) * 5);
    rep_argv[0] = "GenericEmpty";
    rep_argv[1] = "40"; // priority
    asprintf(&rep_argv[2], "%s:%d:%d", "MSG_BUFFER", pipe_in[0], 0);
    asprintf(&rep_argv[3], "%s:%d:%d", "MSG_BUFFER", 0, pipe_out[1]);
    rep_argv[4] = NULL;
  } else {
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

  char snd_buffer[16] = {1};
  char rcv_buffer[16] = {0};
  int retval;
  timestamp_t last;
  while (1) {

    last = generate_timestamp();
    write(pipe_in[1], snd_buffer, sizeof(snd_buffer));

    retval = read(pipe_out[0], rcv_buffer, sizeof(rcv_buffer));

    timestamp_t current = generate_timestamp();

    printf("gev_test_usec (%lf)\n", diff_time(current, last, CPU_MHZ));

    // printf("Read %d bytes %x\n", retval, rcv_buffer[0]);

    usleep(100000); //  10 Hz ish
  }
}
