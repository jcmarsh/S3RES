/*
 * Should start, and wait for a Mapper program to connect (through unix domain sockets).
 * The just send dummy data to exercise the program so that Valgrind can catch errors.
 */

#include "commtypes.h"
#include "replicas.h"
#include "system_config.h"
#include "taslimited.h"

const char* controller_name = "Mapper";

int main(int argc, const char** argv) {
  printf("Usage: No voter -> MapperVoteTest\n");
  printf("       Voter -> MapperVoteTest <Voter_Name> <Redundancy_Level>\n");

  InitTAS(VOTER_PIN, 49);

  pid_t currentPID = 0;
  char** rep_argv;
  int ranger_in[2], maps_out[2], acks_in[2];

  pipe(ranger_in);
  pipe(maps_out);
  pipe(acks_in);

  if (argc == 1) {
    // No Voter
    rep_argv = malloc(sizeof(char *) * 7);
    rep_argv[0] = "Mapper";
    rep_argv[1] = "40";
    rep_argv[2] = "3";
    asprintf(&rep_argv[3], "%s:%d:%d", "RANGE_POSE_DATA", ranger_in[0], 0);
    asprintf(&rep_argv[4], "%s:%d:%d", "MAP_UPDATE", 0, maps_out[1]);
    asprintf(&rep_argv[5], "%s:%d:%d", "COMM_ACK", acks_in[0], 0);
    rep_argv[6] = NULL;
  } else {
    // With Voter
    rep_argv = malloc(sizeof(char *) * 9);
    rep_argv[0] = (char *) argv[1];
    rep_argv[1] = "Mapper";
    rep_argv[2] = (char *) argv[2];
    rep_argv[3] = "800";
    rep_argv[4] = "40";
    asprintf(&rep_argv[5], "%s:%d:%d:%d", "RANGE_POSE_DATA", ranger_in[0], 0, 1);
    asprintf(&rep_argv[6], "%s:%d:%d:%d", "MAP_UPDATE", 0, maps_out[1], 1);
    asprintf(&rep_argv[7], "%s:%d:%d:%d", "COMM_ACK", acks_in[0], 0, 0);
    rep_argv[8] = NULL;
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

  int loops = 100;
  timestamp_t last;
  char buffer[1024];
  struct comm_ack sim_ack;
  sim_ack.hash = 0;
  while (loops--) {
  // Create and send some ranger data
    struct comm_range_pose_data sim_range_data;
    sim_range_data.pose[0] = -2.0;
    sim_range_data.pose[1] = -2.0;
    sim_range_data.pose[2] = 1.0;
    int i = 0;
    for (i = 0; i < 16; i++) {
      sim_range_data.ranges[i] = i * 1.5;
    }

    last = generate_timestamp();
    write(ranger_in[1], &sim_range_data, sizeof(struct comm_range_pose_data));

    // read the map update, should at least have the pose
    read(maps_out[0], buffer, sizeof(buffer));

    timestamp_t current = generate_timestamp();
    //  printf("Pose returned %d, %d\n", ((int *)buffer)[0], ((int *)buffer)[1]);
    printf("map_test_usec (%lf)\n", diff_time(current, last, CPU_MHZ));

    // write the ack
    write(acks_in[1], &sim_ack, sizeof(struct comm_ack));

    sleep(1);
  }
}
