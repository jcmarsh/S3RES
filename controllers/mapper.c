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

// TAS related
cpu_speed_t cpu_speed;

void enterLoop();
int initReplica();

struct point_i* current_pose;
// Count to 3 method worked great before
#define OBS_THRES 3
int obstacle_map[GRID_NUM][GRID_NUM];

struct comm_map_update send_msg;

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

      // unblock the signal (test SDC)
      sigemptyset(&signal_set);
      sigaddset(&signal_set, SIGUSR2);
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

bool insertSDC;
// Need a way to simulate SDC (rare)
void testSDCHandler(int signo) {
  insertSDC = true;
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
  //optOutRT();
  InitTAS(DEFAULT_CPU, &cpu_speed, 15);

  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    perror("Failed to register the restart handler");
    return -1;
  }

  if (signal(SIGUSR2, testSDCHandler) == SIG_ERR) {
    perror("Failed to register the SDC handler");
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
  if (obstacle_map[obs->x][obs->y] > OBS_THRES) {
    // obstacle already there, return false (no changes)
    free(obs);
    return false;
  } else {
    obstacle_map[obs->x][obs->y]++;
    if (obstacle_map[obs->x][obs->y] > OBS_THRES) {
      send_msg.obs_x[send_msg.obs_count] = obs->x;
      send_msg.obs_y[send_msg.obs_count] = obs->y;
      send_msg.obs_count++;
    }
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

  free(current_pose);
  current_pose = gridify(&pose);

  send_msg.obs_count = 0;

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

    addObstacle(gridify(&obstacle_g));
  }

  // TODO: What if a SDC messed with the obs_count sent?
  if (insertSDC) {
    insertSDC = false;
    current_pose->x++;
  }
  send_msg.pose_x = current_pose->x;
  send_msg.pose_y = current_pose->y;
  commSendMapUpdate(pipes[1], &send_msg);
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

  send_msg.pose_x = 0;
  send_msg.pose_y = 0;
  send_msg.obs_count = 0;
  send_msg.obs_x = (int*)malloc(sizeof(int) * 128);
  send_msg.obs_y = (int*)malloc(sizeof(int) * 128);

  enterLoop();

  return 0;
}

