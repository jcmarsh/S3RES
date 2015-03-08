#include <stdbool.h>

// Configuration parameters
#define GRID_NUM 72
#define MAP_SIZE 16
#define OFFSET_X  8
#define OFFSET_Y  8

struct point_i {
  int x;
  int y;
};

struct point_d {
  double x;
  double y;
};

struct node_t {
  int x, y;
  double g_score;
  struct node_t* back_link;
};

struct l_list_t {
  struct node_t* head;
  double sort_val;
  struct l_list_t* tail;
};

struct point_i* gridify(struct point_d* p);
struct point_d* degridify(int x, int y);

void printMap(bool obs_map[][GRID_NUM], struct l_list_t* path);

struct l_list_t* newList();
void printList(struct l_list_t* list);
bool nodeEqauls(struct node_t* a, struct node_t* b);
void removeNode(struct l_list_t** list, struct node_t* node);
void addNode(struct l_list_t** list, struct node_t* node, double sort_val);
struct node_t* findNode(struct l_list_t* list, struct node_t* node);
struct node_t* pop(struct l_list_t** list);
struct node_t* peek(struct l_list_t* list, int num);
void push(struct l_list_t** list, struct node_t* node);
void eraseList(struct l_list_t** list);
struct node_t* newNode(int x, int y, double g_score);