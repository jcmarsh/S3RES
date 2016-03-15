/*
 * A Star controller
 *
 * James Marshall
 */

#include "controller.h"
#include <sys/mman.h>
#include <malloc.h>
#include <math.h>
#include "./inc/mapping.h" // TODO: fix

#define PIPE_COUNT 4

bool obstacle_map[GRID_NUM][GRID_NUM];

// Controller state
struct point_i* goal;

// Position
struct point_i* pose;

struct typed_pipe pipes[PIPE_COUNT]; // Map updates in 0, waypoint request in 1, waypoints out 2
int updates_index, ack_index, way_req_index, way_res_index;
int pipe_count = PIPE_COUNT;

struct l_list_t* goal_path;
struct node_t *current_goal;
struct node_t *n_current_goal;

int priority;
int pinned_cpu;

long fake_hash = 42;

const char* name = "AStar";

void enterLoop(void);
void command(void);
void sendWaypoints(void);

// Set indexes based on pipe types
void setPipeIndexes(void) {
  int i;
  for (i = 0; i < PIPE_COUNT; i++) {
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

struct l_list_t* genNeighbors(struct node_t* node) {
  int i, j;
  struct l_list_t* list = newList();

  for (i = -1; i <= 1; i++) {
    for (j = -1; j <= 1; j++) {
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

bool insertSDC;
bool insertCFE;

int parseArgs(int argc, const char **argv) {
  int i;
  // TODO: error checking
  priority = atoi(argv[1]);
  pipe_count = atoi(argv[2]); // Right now always 4
  if (argc < 6) {
    pid_t currentPID = getpid();
    //connectRecvFDS(currentPID, pipes, PIPE_COUNT, "AStarTest"); // For test purposes
    connectRecvFDS(currentPID, pipes, pipe_count, name, &pinned_cpu);
    setPipeIndexes();
  } else {
    for (i = 0; (i < argc - 3) && (i < PIPE_COUNT); i++) {
      deserializePipe(argv[i + 3], &pipes[i]);
    }
    setPipeIndexes();
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

void eraseAllButList(struct l_list_t** kill_list, struct l_list_t* save_list) {
  struct node_t* clean_n = pop(kill_list);
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

void command(void) {
  struct l_list_t* closed_set = newList();
  struct l_list_t* open_set = newList();

  addNode(&open_set, newNode(pose->x, pose->y, 0), estDistanceG(pose->x, pose->y));
  struct node_t* goal_node = newNode(goal->x, goal->y, 0);
  bool solution = false;

  while(open_set != NULL) { // set empty
    struct node_t* current = pop(&open_set);
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
      eraseList(&open_set);
      free(open_set);
      eraseAllButList(&closed_set, goal_path);      

      solution = true;
      break;
    }

    if (current == NULL) { // TODO: this shouldn't happen.
      //printf("How the #$*& did that happen?\n");
      solution = false;
      break;
    }
    // add current to closedset
    addNode(&closed_set, current, estDistanceG(current->x, current->y));
    struct l_list_t *neighbors = genNeighbors(current);
    struct node_t* curr_neigh = pop(&neighbors);
    while(curr_neigh != NULL) {
      if (findNode(closed_set, curr_neigh) != NULL) {
        // if neighbor in closed - already explored so skip
        free(curr_neigh);
      } else {        
        double tent_g_score = current->g_score + estDistance(curr_neigh->x, curr_neigh->y, current->x, current->y);
        struct node_t* neigh_from_open = findNode(open_set, curr_neigh);
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
}

void sendWaypoints(void) {
  if (current_goal != NULL) {
    free(current_goal);
    current_goal = NULL;
  }
  if (n_current_goal != NULL) {
    free(n_current_goal);
    n_current_goal = NULL;
  }
  struct point_d *n_goal_p;
  struct point_d *goal_p;

  //free(pop(&goal_path)); // trash closest
  current_goal = pop(&goal_path);
  if (current_goal == NULL) { // TODO: these contingences never happen.
    commSendWaypoints(&pipes[way_res_index], 7.0, 7.0, 0.0, 7.0, 7.0, 0.0);
  } else {
    goal_p = degridify(current_goal->x, current_goal->y);
    n_current_goal = pop(&goal_path);
    if (n_current_goal == NULL) {
      // At goal! (Err... right next to it) Just hang out I suppose. By sending same goal twice.
      commSendWaypoints(&pipes[way_res_index], 7.0, 7.0, 0.0, 7.0, 7.0, 0.0);
    } else{
      n_goal_p = degridify(n_current_goal->x, n_current_goal->y);
      commSendWaypoints(&pipes[way_res_index], goal_p->x, goal_p->y, 0.0, n_goal_p->x, n_goal_p->y, 0.0);
      free(n_goal_p);
    }
    free(goal_p);
  }
}

void enterLoop(void) {
  int index;
  int read_ret;
  struct comm_way_req recv_msg_req;
  struct comm_map_update recv_map_update;
  int obs_x[200] = {0}; // TODO: This isn't great... and should be checked I suppose.
  int obs_y[200] = {0};
  recv_map_update.obs_x = obs_x;
  recv_map_update.obs_y = obs_y;

  struct timeval select_timeout;
  fd_set select_set;

  while(1) {
    if (insertCFE) {
      while (1) { }
    }
    
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 0;

    FD_ZERO(&select_set);
    FD_SET(pipes[updates_index].fd_in, &select_set);
    FD_SET(pipes[way_req_index].fd_in, &select_set);

    int retval = select(FD_SETSIZE, &select_set, NULL, NULL, &select_timeout);
    if (retval > 0) {
      if (FD_ISSET(pipes[updates_index].fd_in, &select_set)) {
        read_ret = commRecvMapUpdate(&pipes[updates_index], &recv_map_update);
        if (read_ret > 0) {
          pose->x = recv_map_update.pose_x;
          pose->y = recv_map_update.pose_y;
          for (index = 0; index < recv_map_update.obs_count; index++) {
            obstacle_map[recv_map_update.obs_x[index]][recv_map_update.obs_y[index]] = true;
          }
          if (insertSDC) {
            insertSDC = false;
            fake_hash++;
          }
          if (recv_map_update.obs_count > 0) { // New obstacle arrived
            command();
          }
          commSendAck(&pipes[ack_index], fake_hash);
        } else if (read_ret < 0) {
          perror("AStar - read error on updates_index");
        } else {
          perror("AStar read_ret == 0 on updates_index");
        }
      }
      if (FD_ISSET(pipes[way_req_index].fd_in, &select_set)) {
        read_ret = read(pipes[way_req_index].fd_in, &recv_msg_req, sizeof(struct comm_way_req));
        if (read_ret > 0) { // TODO: Do these calls stack up?
          sendWaypoints();
        } else if (read_ret < 0) {
          perror("AStar - read error on way_req_index");
        } else if (read_ret == 0) {
          perror("AStar read_ret == 0 on way_req_index");
        }
      }
    }
  }
}

int main(int argc, const char **argv) {
  int i, j;

  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initController() < 0) {
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

  for (i = 0; i < GRID_NUM; i++) {
    for (j = 0; j < GRID_NUM; j++) {
      obstacle_map[i][j] = false;
    }
  }

#ifdef __GLIBC__ // musl does not support mallopt. May set as environment variables.
  if (mallopt(M_TRIM_THRESHOLD, -1) != 1) {
    printf("AStar mallopt error, M_TRIM_THRESHOLD\n");
  }
  if (mallopt(M_MMAP_MAX, 0) != 1) {
    printf("AStar mallopt error, M_MMAP_MAX\n");
  }

  // TODO: check if this would prevent page faults in musl builds.
  char *heap_reserve = malloc(sizeof(char) * 1024 * 300); // page in 300K to heap
  free(heap_reserve); // give it back (but reserved for process thanks to mallopt calls)
#endif /* __GLIBC__ */

  enterLoop();

  return 0;
}
