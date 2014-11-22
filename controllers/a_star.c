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


bool obstacle_map[GRID_NUM][GRID_NUM];

// Controller state
// TODO: gridify
double goal[3] = {7.0, 7.0, 0.0};

// Position
double pose[3];

struct typed_pipe pipes[3]; // Map updates in 0, waypoint request in 1, waypoints out 2

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
          if (!obstacle_map[n_x][0] || !obstacle_map[0][n_y]) { // Need to check two adjacent (no jumping gaps!).
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

void enterLoop();
void command();
int initReplica();

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
      connectRecvFDS(currentPID, pipes, 2, "AStar");

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
  if (argc < 3) { // Must request fds

  } else {
    deserializePipe(argv[1], &pipes[0]);
    deserializePipe(argv[2], &pipes[1]);
  }

  return 0;
}

// Should probably separate this out correctly
// Basically the init function
int initReplica() {
  optOutRT();

  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }
  return 0;
}

// straight line distance from goal
double estDistance(int x_1, int y_1, int x_2, int y_2) {
  return sqrt(((x_1 - x_2) * (x_1 - x_2)) + ((y_1 - y_2) * (y_1 - y_2)));
}
double estDistanceG(int x, int y) {
  return estDistance(x, y, goal[0], goal[1]);
}

void command() {
  l_list_t* closed_set = newList();
  l_list_t* open_set = newList();
  addNode(&open_set, newNode(pose[0], pose[1], 0), estDistanceG(pose[0], pose[1]));
  node_t* goal_node = newNode(goal[0], goal[1], 0);
  bool solution = false;

  while(open_set != NULL) { // set empty // TODO: check remove functions
    l_list_t* current = pop(&open_set);
    if (nodeEqauls(current->head, goal_node)) {
      // All done! Extract path
      //printf("SUCCESS!\n");
      node_t* curr = current->head;
      while (curr != NULL) {
        //printf("(%d,%d) <- ", curr->x, curr->y);
        curr = curr->back_link;
      }
      //printf("\n");
      solution = true;
      break;
    }

    // add current to closedset
    node_t* current_n = current->head;
    free(current);
    addNode(&closed_set, current_n, estDistanceG(current_n->x, current_n->y));
    l_list_t *neighbors = genNeighbors(current_n);
    while(neighbors != NULL && neighbors->head != NULL) {
      l_list_t* curr_l = pop(&neighbors);
      node_t* curr_neigh = curr_l->head;
      free(curr_l);
      if (findNode(closed_set, curr_neigh) != NULL) {
        // if neighbor in closed - already explored so skip
      } else {
        double tent_g_score = curr_neigh->g_score + estDistance(curr_neigh->x, curr_neigh->y, current_n->x, current_n->y);
        node_t* neigh_from_open = findNode(open_set, curr_neigh);
        if (neigh_from_open == NULL || tent_g_score < neigh_from_open->g_score) {
          curr_neigh->back_link = current_n;
          curr_neigh->g_score = tent_g_score;
          double f_score = tent_g_score + estDistanceG(curr_neigh->x, curr_neigh->y);
          removeNode(&open_set, curr_neigh); // remove so g_score and f_score are updated.
          addNode(&open_set, curr_neigh, f_score);
        }
      }
    } 
  }

  if (solution) {
    // Write move command (need to de-gridify)
    printf("AStar Solution.\n");
    //commSendWaypoints(pipes[1], goal[0], goal[1], goal[2]);
  } else {
    printf("You have failed, as expected.\n");
    // Not sure what to do in this case
  }
}

void enterLoop() {
  int read_ret;
  struct comm_map_update recv_msg;

  for (int i = 0; i < GRID_NUM; i++) {
    for (int j = 0; j < GRID_NUM; j++) {
      obstacle_map[i][j] = false;
    }
  }

  while(1) {
    // Blocking, but that's okay with me
    read_ret = read(pipes[0].fd_in, &recv_msg, sizeof(struct comm_map_update));
    if (read_ret > 0) {
      obstacle_map[recv_msg.obs_x][recv_msg.obs_y] = true;
      pose[0] = recv_msg.pose_x;
      pose[1] = recv_msg.pose_y;
      command();
    } else if (read_ret == -1) {
      perror("Blocking, eh?");
    } else {
      perror("AStar read_ret == 0?");
    }
  }
}

int main(int argc, const char **argv) {
  /*
  printf("Testing list basics\n");
  l_list_t* open_set = newList();
  printf("Open: %p\n", open_set);
  //addNode(&open_set, newNode(1, 2, 10000), 100);
  //printf("Open: %p\n", open_set);
  addNode(&open_set, newNode(5, 6, 7), 50);
  printf("Open: %p\n", open_set);
  addNode(&open_set, newNode(7, 8, 755555), 160);

  printList(open_set);
  removeNode(&open_set, newNode(5,6,0));
  printList(open_set);
  removeNode(&open_set, newNode(99,88,0));
  printList(open_set);
  removeNode(&open_set, newNode(7,8,0));
  printList(open_set);

  removeNode(&open_set, newNode(1,2,0));
  printList(open_set);

  removeNode(&open_set, newNode(1,2,0));
  printList(open_set);
  */ 

  /* test genNeighbors... neglects test obstacles
  l_list_t* neighs = genNeighbors(newNode(4,4,0));
  printList(neighs);
  */

  /* findNode testing
  if (findNode(open_set, newNode(1, 2, 0)) == NULL) {
    printf("Bad\n");
  } else { printf("GOOD!\n"); }
  if (findNode(open_set, newNode(7, 8, 0)) == NULL) {
    printf("Bad\n");
  } else { printf("GOOD!\n"); }
  if (findNode(open_set, newNode(87, 8, 0)) != NULL) {
    printf("Bad\n");
  } else { printf("GOOD!\n"); }
  */

  /* pop testing
  printList(open_set);
  l_list_t* pop0 = pop(&open_set);
  printList(open_set);
  l_list_t* pop1 = pop(&open_set);
  l_list_t* pop2 = pop(&open_set);
  printList(open_set);

  printf("The poped:\n");
  printList(pop0);
  printList(pop1);
  printList(pop2);

  l_list_t* pop_fail = pop(&open_set); // This segfaults. Not sure how I feel about that.
  */
  
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

