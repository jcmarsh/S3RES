/*
 * A Star controller
 *
 * James Marshall
 */

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "../include/taslimited.h"
#include "../include/commtypes.h"
#include "../include/fd_client.h"
#include "../include/mapping.h"
#include "../include/statstime.h"

#define PIPE_COUNT 4

bool obstacle_map[GRID_NUM][GRID_NUM];

// Controller state
struct point_i* goal;

// Position
struct point_i* pose;

struct typed_pipe pipes[PIPE_COUNT]; // Map updates in 0, waypoint request in 1, waypoints out 2
int updates_index, ack_index, way_req_index, way_res_index;

l_list_t* goal_path;

cpu_speed_t cpu_speed;
int priority;

void enterLoop();
void command();
int initReplica();

// Set indexes based on pipe types
void setPipeIndexes() {
  for (int i = 0; i < PIPE_COUNT; i++) {
    switch (pipes[i].type) {
      case MAP_UPDATE:
        updates_index = i;
        break;
      case COMM_ACK:
        ack_index = i;
        break;
      case WAY_RES:
        way_res_index = i;
        break;
      case WAY_REQ:
        way_req_index = i;
        break;
    }
  }
}

l_list_t* genNeighbors(node_t* node) {
  l_list_t* list = newList();

  for (int i = -1; i <= 1; i++) {
    for (int j = -1; j <= 1; j++) {
      int n_x = node->x + i;
      int n_y = node->y + j;
      if (j == 0 && i == 0) {
        // skip
      } else if ((abs(i) + abs(j)) == 2) { // corner
        if (!obstacle_map[n_x][n_y]) {
          if (!obstacle_map[n_x][node->y] || !obstacle_map[node->x][n_y]) { // Need to check two adjacent (no jumping gaps!).
            addNode(&list, newNode(n_x, n_y, 0), 0.0);
          }
        }
      } else {
        if (!obstacle_map[n_x][n_y]) {
          addNode(&list, newNode(n_x, n_y, 0), 0.0);
        }
      }
    }
  }

  return list;
}

// TODO: move to library
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
      connectRecvFDS(currentPID, pipes, PIPE_COUNT, "AStar");

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

// TODO: move to library?
int parseArgs(int argc, const char **argv) {
  // TODO: error checking
  priority = atoi(argv[1]);
  if (argc < 6) {
    pid_t currentPID = getpid();
    connectRecvFDS(currentPID, pipes, PIPE_COUNT, "AStar");
    setPipeIndexes();
  } else {
    for (int i = 0; (i < argc - 2) && (i < PIPE_COUNT); i++) {
      deserializePipe(argv[i + 2], &pipes[i]);
    }
    setPipeIndexes();
  }

  return 0;
}

// Should probably separate this out correctly
// Basically the init function
int initReplica() {
  // optOutRT();
  InitTAS(DEFAULT_CPU, &cpu_speed, priority);

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

// straight line distance from goal
double estDistance(int x_1, int y_1, int x_2, int y_2) {
  return sqrt(((x_1 - x_2) * (x_1 - x_2)) + ((y_1 - y_2) * (y_1 - y_2)));
}
double estDistanceG(int x, int y) {
  return estDistance(x, y, goal->x, goal->y);
}

void eraseAllButList(l_list_t** kill_list, l_list_t* save_list) {
  node_t* clean_n = pop(kill_list);
  while(clean_n != NULL) {
    if (findNode(save_list, clean_n) != NULL) {
      // Found node in save list, don't free
    } else {
      free(clean_n);
    }
    clean_n = pop(kill_list);
  }
  free(*kill_list);
}

void command() {
  l_list_t* closed_set = newList();
  l_list_t* open_set = newList();

  addNode(&open_set, newNode(pose->x, pose->y, 0), estDistanceG(pose->x, pose->y));
  node_t* goal_node = newNode(goal->x, goal->y, 0);
  bool solution = false;

  while(open_set != NULL) { // set empty // TODO: check remove functions
    node_t* current = pop(&open_set);
    if ((current != NULL) && nodeEqauls(current, goal_node)) {
      free(goal_node);

      // erase old path
      eraseList(&goal_path);

      // All done! Extract path
      while (current != NULL) {
        push(&goal_path, current);
        current = current->back_link;
      }

      // clean up memory
      //eraseAllButList(&open_set, goal_path);
      eraseList(&open_set);
      free(open_set);
      eraseAllButList(&closed_set, goal_path);      

      solution = true;
      break;
    }

    // add current to closedset
    addNode(&closed_set, current, estDistanceG(current->x, current->y));
    l_list_t *neighbors = genNeighbors(current);
    node_t* curr_neigh = pop(&neighbors);
    while(curr_neigh != NULL) {
      if (findNode(closed_set, curr_neigh) != NULL) {
        // if neighbor in closed - already explored so skip
        free(curr_neigh);
      } else {
        double tent_g_score = curr_neigh->g_score + estDistance(curr_neigh->x, curr_neigh->y, current->x, current->y);
        node_t* neigh_from_open = findNode(open_set, curr_neigh);
        if (neigh_from_open == NULL || tent_g_score < neigh_from_open->g_score) {
          curr_neigh->back_link = current;
          curr_neigh->g_score = tent_g_score;
          double f_score = tent_g_score + estDistanceG(curr_neigh->x, curr_neigh->y);
          removeNode(&open_set, curr_neigh); // remove so g_score and f_score are updated.
          addNode(&open_set, curr_neigh, f_score);
        } else {
          free(curr_neigh);
        }
      }
      curr_neigh = pop(&neighbors);
    }
    free(neighbors);
  }

  if (!solution) {
    printf("You have failed, as expected.\n");
    // Not sure what to do in this case
  }
}

void enterLoop() {
  int read_ret;
  int recv_msg_buffer[1024] = {0};
  struct comm_way_req recv_msg_req;

  struct timeval select_timeout;
  fd_set select_set;

  while(1) {
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    FD_ZERO(&select_set);
    FD_SET(pipes[updates_index].fd_in, &select_set);
    FD_SET(pipes[way_req_index].fd_in, &select_set);

    int retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    if (retval > 0) {
      if (FD_ISSET(pipes[updates_index].fd_in, &select_set)) {
        read_ret = read(pipes[updates_index].fd_in, &recv_msg_buffer, sizeof(recv_msg_buffer));
        if (read_ret > 0) {
          pose->x = recv_msg_buffer[0];
          pose->y = recv_msg_buffer[1];
          int obs_index = 3;
          for (int index = 0; index < recv_msg_buffer[2]; index++) {
            int obs_x = recv_msg_buffer[obs_index + (index * 2)];
            int obs_y = recv_msg_buffer[obs_index + (index * 2 + 1)];
            obstacle_map[obs_x][obs_y] = true;
          }
          if (recv_msg_buffer[2] > 0) { // New obstacle arrived
            command();
          }
          commSendAck(pipes[ack_index]);
        } else if (read_ret == -1) {
          perror("AStar - read blocking");
        } else {
          perror("AStar read_ret == 0?");
        }
      }
      if (FD_ISSET(pipes[way_req_index].fd_in, &select_set)) {
        read_ret = read(pipes[way_req_index].fd_in, &recv_msg_req, sizeof(struct comm_way_req));
        if (read_ret > 0) {
          if (goal_path->head == NULL) {
            commSendWaypoints(pipes[way_res_index], -7.0, -7.0, 0.0);
            // none yet
          } else {
            free(pop(&goal_path)); // Toss closest result
            node_t* new_goal = pop(&goal_path);
            point_d *goal_p = degridify(new_goal->x, new_goal->y);
            free(new_goal);
            if (insertSDC) {
              insertSDC = false;
              goal_p->x++;
            }
            commSendWaypoints(pipes[way_res_index], goal_p->x, goal_p->y, 0.0);
            free(goal_p);
          }
        } else if (read_ret == -1) {
          perror("AStar - read blocking");
        } else {
          perror("AStar read_ret == 0?");
          exit(0);
        }
      }  
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

  pose = (struct point_i*) malloc(sizeof(struct point_i));
  struct point_d* goal_d = (struct point_d*) malloc(sizeof(struct point_d));
  goal_d->x = 7.0;
  goal_d->y = 7.0;
  goal = gridify(goal_d);
  free(goal_d);
  goal_path = newList();

  for (int i = 0; i < GRID_NUM; i++) {
    for (int j = 0; j < GRID_NUM; j++) {
      obstacle_map[i][j] = false;
    }
  }

  enterLoop();

  return 0;
}

