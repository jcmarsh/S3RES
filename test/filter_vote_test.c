// Test filter

#include "test.h"

const char* controller_name = "Filter";

int main(int argc, const char** argv) {
  // VoterM Filter TMR 400 80 RANGE_POSE_DATA:3:0:1 RANGE_POSE_DATA:0:19:1 RANGE_POSE_DATA:0:21:0 RANGE_POSE_DATA:0:23:0

  pid_t currentPID = 0;
  char** rep_argv;
  int pipe_in[2], pipe_out0[2], pipe_out1[2], pipe_out2[2];

  pipe(pipe_in);
  pipe(pipe_out0);
  pipe(pipe_out1);
  pipe(pipe_out2);

  rep_argv = malloc(sizeof(char *) * 10);
  //  rep_argv[0] = "VoterM";
  rep_argv[0] = "VoterD";
  rep_argv[1] = "Filter";
  rep_argv[2] = "TMR";
  rep_argv[3] = "800"; // Timeout
  rep_argv[4] = "80";  // priority
  //  rep_argv[3] = "1800";
  //  rep_argv[4] = "1";
  asprintf(&rep_argv[5], "%s:%d:%d:%d", "RANGE_POSE_DATA", pipe_in[0], 0, 1);
  asprintf(&rep_argv[6], "%s:%d:%d:%d", "RANGE_POSE_DATA", 0, pipe_out0[1], 1);
  asprintf(&rep_argv[7], "%s:%d:%d:%d", "RANGE_POSE_DATA", 0, pipe_out1[1], 0);
  asprintf(&rep_argv[8], "%s:%d:%d:%d", "RANGE_POSE_DATA", 0, pipe_out2[1], 0);
  rep_argv[9] = NULL;

  // Need to fork exec VoterM, but first need a pipe in and two outs.
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      if (-1 == execv(rep_argv[0], rep_argv)) {
	printf("Exec ERROR!\n");
      }
    }
  }

  sleep(2);

  // Should be connected now.
  double pose_add = 0.0;
  int loops = 10000000;
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

    printf("Writing the datas.\n");
    write(pipe_in[1], &sim_range_data, sizeof(struct comm_range_pose_data));

    // read filtered data
    read(pipe_out0[0], &sim_range_data, sizeof(sim_range_data));
    printf("Does order here matter?\n");
    read(pipe_out1[0], &sim_range_data, sizeof(sim_range_data));
    read(pipe_out2[0], &sim_range_data, sizeof(sim_range_data));
    printf("Pose returned %f, %f\n", sim_range_data.pose[0], sim_range_data.pose[1]);

    //usleep(1000000);
    sleep(1);
  }
}
