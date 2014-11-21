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
#include "../include/statstime.h"
#include "../include/fd_client.h"

// Configuration parameters
#define GRID_NUM 64

bool obstacle_map[GRID_NUM][GRID_NUM];

// Controller state
double goal[3] = {7.0, 7.0, 0.0};

// Position
double pose[3];

struct typed_pipe pipes[2]; // Map updates in 0, waypoints out 1

//typedef struct node_t node_t;
//typedef struct l_list_t l_list_t;

struct node_t {
  int x, y;
  double g_score;
  node_t* back_link;
};

struct l_list_t {
  node_t* head;
  double sort_val;
  l_list_t* tail;
};

l_list_t* newList() {
  l_list_t* new_list = (l_list_t*) malloc(sizeof(l_list_t));
  new_list->head = NULL;
  new_list->sort_val = 0.0;
  new_list->tail = NULL;
  return new_list;
}

void addNode(l_list_t** list, node_t* node, double sort_val) {
  if ((*list)->head == NULL) {
    // Empty list: add node to head
    (*list)->head = node;
    (*list)->sort_val = sort_val;
  } else if ((*list)->sort_val > sort_val) {
    // current is greater than new: swap
    l_list_t* new_list = newList();
    new_list->head = (*list)->head;
    (*list)->head = node;
    new_list->sort_val = (*list)->sort_val;
    (*list)->sort_val = sort_val;
    new_list->tail = (*list)->tail;
    (*list)->tail = new_list;
  } else {
    // current is smaller: keep going
    if ((*list)->tail == NULL) {
      (*list)->tail = newList();
    }
    addNode(&((*list)->tail), node, sort_val);
  }
}

bool nodeEqauls(node_t* a, node_t* b) {
  return ((a->x == b->x) && (a->y == b->y));
}

bool findNode(l_list_t* list, node_t* node) {
  if (list == NULL || list->head == NULL) {
    return false; // not found
  } else if (nodeEqauls(node, list->head)) {
    return true;
  } else {
    return findNode(list->tail, node);
  }
}

l_list_t* pop(l_list_t** list) {
  l_list_t* ret_val = *list;
  *list = (*list)->tail;
  ret_val->tail = NULL;
  return ret_val;
}

void printList(l_list_t* list) {
  if (list == NULL || list->head == NULL) {
    printf("X\n");
  } else {
    printf("(Val: %f, (%d,%d)) -> ", list->sort_val, list->head->x, list->head->y);
    printList(list->tail);
  }
}

node_t* newNode(int x, int y, double g_score) {
  node_t* new_node = (node_t*) malloc(sizeof(node_t));
  new_node->x = x;
  new_node->y = y;
  new_node->g_score = g_score;
  new_node->back_link = NULL;
  return new_node;
}

l_list_t* genNeighbors(node_t* node) {
  l_list_t* list = newList();

  node_t* n_node;

  for (int i = -1; i <= 1; i++) {
    for (int j = -1; j <= 1; j++) {
      int n_x = n_node->x + i;
      int n_y = n_node->y + j;
      if (j == 0 && i == 0) {
        // skip
      } else if ((abs(i) + abs(j)) == 2) { // corner
        if (!obstacle_map[n_x][n_y]) {
          // Need to check two adjacent (no jumping gaps!).
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
  if (signal(SIGUSR1, restartHandler) == SIG_ERR) {
    puts("Failed to register the restart handler");
    return -1;
  }
  return 0;
}

// straight line distance from goal
double estDistance(int x, int y) {
  return sqrt(((x - goal[0]) * (x - goal[0])) + ((y - goal[1]) * (y - goal[1])));
}

void command() {
  l_list_t* closed_set = newList();
  l_list_t* open_set = newList();
  addNode(&open_set, newNode(pose[0], pose[1], 0), estDistance(pose[0], pose[1]));
  node_t* goal_node = newNode(goal[0], goal[1], 0);

  while(open_set != NULL) { // set empty // TODO: check remove functions
    l_list_t* current = pop(&open_set);
    if (nodeEqauls(current->head, goal_node)) {
      printf("SUCCESS!\n");
      // All done! Extract path
    }

    // add current to closedset
    node_t* current_n = current->head;
    free(current);
    addNode(&closed_set, current_n, estDistance(current_n->x, current_n->y));

    // for each neighbor of current
      // if neighbor in closed - already explored so skip
      // calculate tent_g_score 

  }

  // Write move command (need to de-gridify)
  commSendWaypoints(pipes[1], goal[0], goal[1], goal[2]);
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
      perror("ArtPot read_ret == 0?");
    }
  }
}

int main(int argc, const char **argv) {
  printf("Testing list basics\n");
  l_list_t* open_set = newList();
  printf("Open: %p\n", open_set);
  addNode(&open_set, newNode(1, 2, 10000), 100);
  printf("Open: %p\n", open_set);
  addNode(&open_set, newNode(5, 6, 7), 50);
  printf("Open: %p\n", open_set);
  addNode(&open_set, newNode(7, 8, 755555), 160);

  if (!findNode(open_set, newNode(1, 2, 0))) {
    printf("Bad\n");
  } else { printf("GOOD!\n"); }
  if (!findNode(open_set, newNode(7, 8, 0))) {
    printf("Bad\n");
  } else { printf("GOOD!\n"); }
  if (findNode(open_set, newNode(87, 8, 0))) {
    printf("Bad\n");
  } else { printf("GOOD!\n"); }

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

  l_list_t* pop_fail = pop(&open_set);

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

