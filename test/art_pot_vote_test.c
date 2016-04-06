// Test filter

#include "commtypes.h"
#include "replicas.h"
#include "system_config.h"
#include "taslimited.h"

const char* controller_name = "ArtPot";

int main(int argc, const char** argv) {
  printf("Usage: No voter -> ArtPotVoteTest\n");
  printf("       Voter -> ArtPotVoteTest <Voter_Name> <Redundancy_Level>\n");

  InitTAS(VOTER_PIN, 49);

  pid_t currentPID = 0;
  char** rep_argv;
  int ranger_in[2], way_out[2], way_in[2], move_out[2];

  pipe(ranger_in);
  pipe(way_out);
  pipe(way_in);
  pipe(move_out);

  rep_argv = malloc(sizeof(char *) * 10);

  if (argc == 1) {
    // No Voter
    rep_argv = malloc(sizeof(char *) * 8);
    rep_argv[0] = "ArtPot";
    rep_argv[1] = "40";
    rep_argv[2] = "4";
    asprintf(&rep_argv[3], "%s:%d:%d", "RANGE_POSE_DATA", ranger_in[0], 0);
    asprintf(&rep_argv[4], "%s:%d:%d", "WAY_REQ", 0, way_out[1]);
    asprintf(&rep_argv[5], "%s:%d:%d", "WAY_RES", way_in[0], 0);
    asprintf(&rep_argv[6], "%s:%d:%d", "MOV_CMD", 0, move_out[1]);
    rep_argv[7] = NULL;
  } else {
    // With Voter
    rep_argv = malloc(sizeof(char *) * 10);
    rep_argv[0] = argv[1];
    rep_argv[1] = "ArtPot";
    rep_argv[2] = argv[2];
    rep_argv[3] = "2000"; // Timeout
    rep_argv[4] = "80";  // priority
    asprintf(&rep_argv[5], "%s:%d:%d:%d", "RANGE_POSE_DATA", ranger_in[0], 0, 1);
    asprintf(&rep_argv[6], "%s:%d:%d:%d", "WAY_REQ", 0, way_out[1], 0);
    asprintf(&rep_argv[7], "%s:%d:%d:%d", "WAY_RES", way_in[0], 0, 0);
    asprintf(&rep_argv[8], "%s:%d:%d:%d", "MOV_CMD", 0, move_out[1], 1);
    rep_argv[9] = NULL;
  }

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

  struct comm_way_req way_req;
  struct comm_way_res way_res;

  int loops = 100;
  timestamp_t last;
  while (loops--) {
    // Create and send some ranger data
    struct comm_range_pose_data sim_range_data;
    sim_range_data.pose[0] = -7.0;
    sim_range_data.pose[1] = -7.0;
    sim_range_data.pose[2] = 1.0;
    int i = 0;
    for (i = 0; i < 16; i++) {
      sim_range_data.ranges[i] = i * 1.5;
    }

    last = generate_timestamp();
    write(ranger_in[1], &sim_range_data, sizeof(struct comm_range_pose_data));

    // Uncomment this to create havoc for VoterM (but not VoterD)
    //write(ranger_in[1], &sim_range_data, sizeof(struct comm_range_pose_data));
    //write(ranger_in[1], &sim_range_data, sizeof(struct comm_range_pose_data));
    //write(ranger_in[1], &sim_range_data, sizeof(struct comm_range_pose_data));

    // check for command out or a new waypoint request
    int retval = 0;

    struct timeval select_timeout;
    fd_set select_set;

    // See if any of the read pipes have anything
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    FD_ZERO(&select_set);
    FD_SET(way_out[0], &select_set);
    FD_SET(move_out[0], &select_set);

    retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);

    if (retval > 0) {
      if (FD_ISSET(way_out[0], &select_set)) {
        read(way_out[0], &way_req, sizeof(way_req));
        // printf("Got waypoint req\n");
        way_res.point[0] = 8.0;
        way_res.point[1] = 8.0;
        way_res.point[2] = 0.0;
        write(way_in[1], &way_res, sizeof(way_res));
      }
      if (FD_ISSET(move_out[0], &select_set)) {
        // read the command out
        struct comm_mov_cmd mov_cmd;
        read(move_out[0], &mov_cmd, sizeof(mov_cmd));

        timestamp_t current = generate_timestamp();

        printf("art_test_usec (%lf)\n", diff_time(current, last, CPU_MHZ));
        //printf("Move Command: %f, %f\n", mov_cmd.vel_cmd[0], mov_cmd.vel_cmd[1]);
      }
    }
    sleep(1);
  }
}
