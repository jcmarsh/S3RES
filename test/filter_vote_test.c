// Test filter

#include "commtypes.h"
#include "replicas.h"
#include "system_config.h"
#include "taslimited.h"

const char* controller_name = "Filter";

int main(int argc, const char** argv) {
  printf("Usage: No voter -> FilterVoteTest\n");
  printf("       Voter -> FilterVoteTest <Voter_Name> <Redundancy_Level>\n");

  pid_t currentPID = 0;
  char** rep_argv;
  int pipe_in[2], pipe_out0[2], pipe_out1[2], pipe_out2[2];

  pipe(pipe_in);
  pipe(pipe_out0);
  pipe(pipe_out1);
  pipe(pipe_out2);

  if (argc == 1) {
    // No Voter
    rep_argv = malloc(sizeof(char *) * 8);
    rep_argv[0] = "Filter";
    rep_argv[1] = "80"; // priority
    rep_argv[2] = "4"; // Pipe Count
    asprintf(&rep_argv[3], "%s:%d:%d", "RANGE_POSE_DATA", pipe_in[0], 0);
    asprintf(&rep_argv[4], "%s:%d:%d", "RANGE_POSE_DATA", 0, pipe_out0[1]);
    asprintf(&rep_argv[5], "%s:%d:%d", "RANGE_POSE_DATA", 0, pipe_out1[1]);
    asprintf(&rep_argv[6], "%s:%d:%d", "RANGE_POSE_DATA", 0, pipe_out2[1]);
    rep_argv[7] = NULL;
  } else {
    // With Voter
    rep_argv = malloc(sizeof(char *) * 10);
    rep_argv[0] = argv[1]; // VoterM or Voterd
    rep_argv[1] = "Filter";
    rep_argv[2] = argv[2]; // SMR, DMR, or TMR
    rep_argv[3] = "800"; // Timeout
    rep_argv[4] = "80";  // priority
    asprintf(&rep_argv[5], "%s:%d:%d:%d", "RANGE_POSE_DATA", pipe_in[0], 0, 1);
    asprintf(&rep_argv[6], "%s:%d:%d:%d", "RANGE_POSE_DATA", 0, pipe_out0[1], 0);
    asprintf(&rep_argv[7], "%s:%d:%d:%d", "RANGE_POSE_DATA", 0, pipe_out1[1], 0);
    asprintf(&rep_argv[8], "%s:%d:%d:%d", "RANGE_POSE_DATA", 0, pipe_out2[1], 1);
    rep_argv[9] = NULL;
  }

  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      if (-1 == execv(rep_argv[0], rep_argv)) {
        printf("Exec ERROR!\n");
      }
    }
  }

  sleep(2);

  double pose_add = 0.0;
  int loops = 10000000;
  timestamp_t last;
  while (loops--) {
    // Create and send some ranger data
    struct comm_range_pose_data sim_range_data;
    sim_range_data.pose[0] = -2.0 + pose_add++;
    sim_range_data.pose[1] = -5.0;
    sim_range_data.pose[2] = 1.0;
    int i = 0;
    for (i = 0; i < 16; i++) {
      sim_range_data.ranges[i] = i * 1.5;
    }

    last = generate_timestamp();
    write(pipe_in[1], &sim_range_data, sizeof(struct comm_range_pose_data));

    // read filtered data
    read(pipe_out0[0], &sim_range_data, sizeof(sim_range_data));
    read(pipe_out1[0], &sim_range_data, sizeof(sim_range_data));
    read(pipe_out2[0], &sim_range_data, sizeof(sim_range_data));

    timestamp_t current = generate_timestamp();

    printf("fil_test_usec (%lf)\n", diff_time(current, last, CPU_MHZ));
    //printf("Pose returned %f, %f\n", sim_range_data.pose[0], sim_range_data.pose[1]);

    //usleep(1000000);
    sleep(1);
  }
}
