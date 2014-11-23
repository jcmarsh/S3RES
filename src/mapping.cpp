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

/*              */
l_list_t* newList() {
  l_list_t* new_list = (l_list_t*) malloc(sizeof(l_list_t));
  new_list->head = NULL;
  new_list->sort_val = 0.0;
  new_list->tail = NULL;
  return new_list;
}

void printList(l_list_t* list) {
  if (list == NULL) {
    printf("N\n");
  } else if (list->head == NULL) {
    printf("X\n");
  } else {
    printf("(Val: %f, (%d,%d)) -> ", list->sort_val, list->head->x, list->head->y);
    printList(list->tail);
  }
}

bool nodeEqauls(node_t* a, node_t* b) {
  return ((a->x == b->x) && (a->y == b->y));
}

void removeNode(l_list_t** list, node_t* node) {
  if ((*list) == NULL || (*list)->head == NULL) {
    return;
  } else if (nodeEqauls((*list)->head, node)) {
    // Magic happens
    free((*list)->head);
    l_list_t* removed = (*list);
    (*list) = (*list)->tail;
    free(removed);
    return;
  } else {
    // keep looking
    return (removeNode(&((*list)->tail), node));
  }
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

node_t* findNode(l_list_t* list, node_t* node) {
  if (list == NULL || list->head == NULL) {
    return NULL; // not found
  } else if (nodeEqauls(node, list->head)) {
    return list->head;
  } else {
    return findNode(list->tail, node);
  }
}

// SHOULD NEVER BE USED IN CONJUNCTION WITH addNode
void push(l_list_t** list, node_t* node) {
  l_list_t* new_list = newList();
  new_list->head = node;
  new_list->sort_val = 0.0; // Not used. Do not use with addNode
  new_list->tail = *list;
  (*list) = new_list;
}

node_t* pop(l_list_t** list) {
  l_list_t* popped = *list;
  node_t* ret_val = popped->head;
  free(popped);

  *list = (*list)->tail;
  if ((*list) == NULL) { // Never an empty list
    (*list) = newList();
  }
  
  return ret_val;
}

node_t* newNode(int x, int y, double g_score) {
  node_t* new_node = (node_t*) malloc(sizeof(node_t));
  new_node->x = x;
  new_node->y = y;
  new_node->g_score = g_score;
  new_node->back_link = NULL;
  return new_node;
}