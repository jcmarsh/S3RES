// Test AStar

#include "commtypes.h"
#include "replicas.h"
#include "system_config.h"
#include "taslimited.h"

const char* controller_name = "AStar";

int main(int argc, const char** argv) {
  printf("Usage: No voter -> AStarVoteTest\n");
  printf("       Voter -> AStarVoteTest <Voter_Name> <Redundancy_Level>\n");

  pid_t currentPID = 0;
  char** rep_argv;
  int map_in[2], ack_out[2], way_req_in[2], way_res_out[2];

  pipe(map_in);
  pipe(ack_out);
  pipe(way_req_in);
  pipe(way_res_out);

  if (argc == 1) {
    // No Voter
    rep_argv = malloc(sizeof(char *) * 8);
    rep_argv[0] = "AStar";
    rep_argv[1] = "80"; // priority
    rep_argv[2] = "4"; // Pipe Count
    asprintf(&rep_argv[3], "%s:%d:%d", "MAP_UPDATE", map_in[0], 0);
    asprintf(&rep_argv[4], "%s:%d:%d", "COMM_ACK", 0, ack_out[1]);
    asprintf(&rep_argv[5], "%s:%d:%d", "WAY_REQ", way_req_in[0], 0);
    asprintf(&rep_argv[6], "%s:%d:%d", "WAY_RES", 0, way_res_out[1]);
    rep_argv[7] = NULL;
  } else {
    // With Voter
    rep_argv = malloc(sizeof(char *) * 10);
    rep_argv[0] = argv[1]; // VoterM or Voterd
    rep_argv[1] = "AStar";
    rep_argv[2] = argv[2]; // SMR, DMR, or TMR
    rep_argv[3] = "80000"; // Timeout
    rep_argv[4] = "80";  // priority
    asprintf(&rep_argv[5], "%s:%d:%d:%d", "MAP_UPDATE", map_in[0], 0, 1);
    asprintf(&rep_argv[6], "%s:%d:%d:%d", "COMM_ACK", 0, ack_out[1], 1);
    asprintf(&rep_argv[7], "%s:%d:%d:%d", "WAY_REQ", way_req_in[0], 0, 2);
    asprintf(&rep_argv[8], "%s:%d:%d:%d", "WAY_RES", 0, way_res_out[1], 2);
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

  int loops = 100;
  int c = 0;
  int i;
  timestamp_t last;
  struct comm_way_req way_req;
  struct comm_way_res way_res;
  while (loops--) {
    // send waypoint request
    write(way_req_in[1], &way_req, sizeof(way_req));
    read(way_res_out[0], &way_res, sizeof(way_res));
    printf("Waypoint: %f, %f\n", way_res.point[0], way_res.point[1]);

    // Send Map update
    // x y pose, obstacle count (5), and x y for each obstacle
    int buffer[13] = {4, 5, 5, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4};

    last = generate_timestamp();
    write(map_in[1], buffer, sizeof(int) * 13);

    //write(way_req_in[1], &way_req, sizeof(way_req));
    //write(way_req_in[1], &way_req, sizeof(way_req));

    // Get ack
    struct comm_ack ack;
    read(ack_out[0], &ack, sizeof(ack));
    
    timestamp_t current = generate_timestamp();

    printf("ast_test_usec (%lf)\n", diff_time(current, last, CPU_MHZ));
    read(way_res_out[0], &way_res, sizeof(way_res));
    printf("Waypoint: %f, %f\n", way_res.point[0], way_res.point[1]);

    // send waypoint request
    //write(way_req_in[1], &way_req, sizeof(way_req));
    write(way_req_in[1], &way_req, sizeof(way_req));
    // get waypoint response
    read(way_res_out[0], &way_res, sizeof(way_res));
    printf("Waypoint: %f, %f\n", way_res.point[0], way_res.point[1]);

    write(way_req_in[1], &way_req, sizeof(way_req));
    read(way_res_out[0], &way_res, sizeof(way_res));    
    printf("Waypoint: %f, %f\n", way_res.point[0], way_res.point[1]);

  	sleep(1);
  }
}
