/*
 * Builds a map of obstacles
 *
 * James Marshall
 */

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "../include/commtypes.h"
#include "../include/fd_client.h"
#include "../include/mapping.h"
#include "../include/statstime.h"
#include "../include/taslimited.h"

#define RANGE_COUNT 16

struct typed_pipe pipes[2];

FILE * out_file;

void enterLoop();
int initReplica();

struct point_i* current_pose;
// Count to 3 method worked great before
bool obstacle_map[GRID_NUM][GRID_NUM];

void restartHandler(int signo) {
  pid_t currentPID = 0;
  // fork
  currentPID = fork();

  if (currentPID >= 0) { // Successful fork
    if (currentPID == 0) { // Child process
      // child sets new id, recreates connects, loops
      initReplica();
      // Get own pid, send to voter
      currentPID = getpid();
      connectRecvFDS(currentPID, pipes, 2, "Mapper");
      
      // unblock the signal
      sigset_t signal_set;
      sigemptyset(&signal_set);
      sigaddset(&signal_set, SIGUSR1);
      sigprocmask(SIG_UNBLOCK, &signal_set, NULL);

      enterLoop(); // return to normal
    } else {   // Parent just returns
      return;
    }
  } else {
    printf("Fork error!\n");
    return;
  }
}

int parseArgs(int argc, const char **argv) {
  // TODO: error checking
  if (argc < 2) { // Must request fds
    printf("Mapper usage message.\n");
  } else {
    deserializePipe(argv[1], &pipes[0]);
    deserializePipe(argv[2], &pipes[1]);
  }

  return 0;
}

// TODO: Should probably separate this out correctly
// Basically the init function
int initReplica() {
  optOutRT();

  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }
  return 0;
}

// return true if something changed
bool addObstacle(struct point_i* obs) {
  if (obs->x < 0 || obs->x > GRID_NUM || obs->y < 0 || obs->y > GRID_NUM) {
    // erroneous, ignore
    free(obs);
    return false;
  }
  if (obstacle_map[obs->x][obs->y]) {
    // obstacle already there, return false (no changes)
    free(obs);
    return false;
  } else {
    obstacle_map[obs->x][obs->y] = true;
    commSendMapUpdate(pipes[1], obs->x, obs->y, current_pose->x, current_pose->y);
    free(obs);
    return true;
  }
}

void updateMap(struct comm_range_pose_data * data) {
  double theta_pose;
  // Read pose
  struct point_d pose;
  pose.x = data->pose[INDEX_X];
  pose.y = data->pose[INDEX_Y];
  theta_pose = data->pose[INDEX_A];

  bool changed = false;

  free(current_pose);
  current_pose = gridify(&pose);

  // Convert ranges absolute positions
  for (int i = 0; i < RANGE_COUNT; i++) {
    struct point_d obstacle_l, obstacle_g;

    // obstacle location relative to the robot
    double tao = (2.0 * M_PI * i) / RANGE_COUNT;
    obstacle_l.x = data->ranges[i] * cos(tao);
    obstacle_l.y = data->ranges[i] * sin(tao);
    
    // obstacle location in global coords
    obstacle_g.x = obstacle_l.x * cos(theta_pose) - obstacle_l.y * sin(theta_pose);
    obstacle_g.x += pose.x;
    obstacle_g.y = obstacle_l.x * sin(theta_pose) + obstacle_l.y * cos(theta_pose);
    obstacle_g.y += pose.y;



    changed = addObstacle(gridify(&obstacle_g)) || changed;
  }
}

void enterLoop() {
  int read_ret;
  struct comm_range_pose_data recv_msg;

  for (int i = 0; i < GRID_NUM; i++) {
    for (int j = 0; j < GRID_NUM; j++) {
      obstacle_map[i][j] = false;
    }
  }
 
  while(1) {
    // Blocking, but that's okay with me
    read_ret = read(pipes[0].fd_in, &recv_msg, sizeof(struct comm_range_pose_data));
    if (read_ret > 0) {
      // TODO: Error checking
      updateMap(&recv_msg);
    } else if (read_ret == -1) {
      perror("Blocking, eh?");
    } else {
      puts("Mapper read_ret == 0?");
    }
  }
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initReplica() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  enterLoop();

  return 0;
}

