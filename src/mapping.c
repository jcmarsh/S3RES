#include <stdlib.h>
#include <stdio.h>
#include "../include/mapping.h"

/*               */
struct point_i* gridify(struct point_d* p) {
  struct point_i* new_point = (struct point_i*) malloc(sizeof(struct point_i));
  double interval = MAP_SIZE / (double)GRID_NUM;
  new_point->x = (int)((p->x + OFFSET_X) / interval);
  new_point->y = (int)((p->y + OFFSET_Y) / interval);

  // Account for edge of map
  if (new_point->x == GRID_NUM) {
    new_point->x--;
  }
  if (new_point->y == GRID_NUM) {
    new_point->y--;
  }

  return new_point;
}

struct point_d* degridify(int x, int y) {
  struct point_d* new_point = (struct point_d*) malloc(sizeof(struct point_d));
  double interval = MAP_SIZE / (double)GRID_NUM;
  new_point->x = x * interval + (interval / 2.0) - OFFSET_X;
  new_point->y = y * interval + (interval / 2.0) - OFFSET_Y;
  return new_point;
}

void printMap(bool obs_map[][GRID_NUM], struct l_list_t* path) {
  int i, j;
  printf("\n");
  for (i = GRID_NUM - 1; i >= 0; i--) {
    for (j = 0; j < GRID_NUM; j++) {
      struct node_t* node = newNode(j, i, 0);
      if (findNode(path, node) != NULL) {
        if (obs_map[j][i]) {
          printf("!"); // Should not happen
        } else {
          printf("o");
        }
      } else {
        if (obs_map[j][i]) {
          printf("X");
        } else {
          printf(".");
        }
      }
      free(node);
    }
    printf("\n");
  }
}

/*              */
struct l_list_t* newList() {
  struct l_list_t* new_list = (struct l_list_t*) malloc(sizeof(struct l_list_t));
  new_list->head = NULL;
  new_list->sort_val = 0.0;
  new_list->tail = NULL;
  return new_list;
}

void printList(struct l_list_t* list) {
  if (list == NULL) {
    printf("N\n");
  } else if (list->head == NULL) {
    printf("X\n");
  } else {
    printf("(Val: %f, (%d,%d)) -> ", list->sort_val, list->head->x, list->head->y);
    printList(list->tail);
  }
}

bool nodeEqauls(struct node_t* a, struct node_t* b) {
  if (a == NULL || b == NULL) {
    return false;
  } else {
    return ((a->x == b->x) && (a->y == b->y));
  }
}

// Frees the removed node
void removeNode(struct l_list_t** list, struct node_t* node) {
  if ((*list) == NULL || (*list)->head == NULL) {
    return;
  } else if (nodeEqauls((*list)->head, node)) {
    // Magic happens
    free((*list)->head);
    struct l_list_t* removed = (*list);
    (*list) = (*list)->tail;
    free(removed);
    return;
  } else {
    // keep looking
    return (removeNode(&((*list)->tail), node));
  }
}

// node here has already been allocated
void addNode(struct l_list_t** list, struct node_t* node, double sort_val) {
  if ((*list)->head == NULL) {
    // Empty list: add node to head
    (*list)->head = node;
    (*list)->sort_val = sort_val;
  } else if ((*list)->sort_val > sort_val) {
    // current is greater than new: swap
    struct l_list_t* new_list = newList();
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

struct node_t* findNode(struct l_list_t* list, struct node_t* node) {
  if (list == NULL || list->head == NULL) {
    return NULL; // not found
  } else if (nodeEqauls(node, list->head)) {
    return list->head;
  } else {
    return findNode(list->tail, node);
  }
}

// SHOULD NEVER BE USED IN CONJUNCTION WITH addNode
void push(struct l_list_t** list, struct node_t* node) {
  struct l_list_t* new_list = newList();
  new_list->head = node;
  new_list->sort_val = 0.0; // Not used. Do not use with addNode
  new_list->tail = *list;
  (*list) = new_list;
}

struct node_t* pop(struct l_list_t** list) {
  struct l_list_t* popped = *list;
  struct node_t* ret_val = popped->head;
  
  *list = popped->tail;
  free(popped);

  if ((*list) == NULL) { // Never an empty list
    (*list) = newList();
  }
  
  return ret_val;
}

// returns the num (from the top) without modifying anything
struct node_t* peek(struct l_list_t* list, int num) {
  if (list == NULL || list->head == NULL) {
    return NULL; // not found
  } else if (num == 0) {
    return list->head;
  } else {
    return peek(list->tail, num - 1);
  }
}

void eraseList(struct l_list_t** list) {
  struct node_t* curr = pop(list);
  while (curr != NULL) {
    free(curr);
    curr = pop(list);
  }
}

struct node_t* newNode(int x, int y, double g_score) {
  struct node_t* new_node = (struct node_t*) malloc(sizeof(struct node_t));
  new_node->x = x;
  new_node->y = y;
  new_node->g_score = g_score;
  new_node->back_link = NULL;
  return new_node;
}