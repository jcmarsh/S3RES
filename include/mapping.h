// Configuration parameters
#define GRID_NUM 64
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
  node_t* back_link;
};

struct l_list_t {
  node_t* head;
  double sort_val;
  l_list_t* tail;
};

struct point_i* gridify(struct point_d* p);
struct point_d* degridify(int x, int y);

void printMap(bool obs_map[][GRID_NUM], l_list_t* path);

l_list_t* newList();
void printList(l_list_t* list);
bool nodeEqauls(node_t* a, node_t* b);
void removeNode(l_list_t** list, node_t* node);
void addNode(l_list_t** list, node_t* node, double sort_val);
node_t* findNode(l_list_t* list, node_t* node);
node_t* pop(l_list_t** list);
void push(l_list_t** list, node_t* node);
void eraseList(l_list_t** list);
node_t* newNode(int x, int y, double g_score);