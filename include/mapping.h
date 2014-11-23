// Configuration parameters
#define GRID_NUM 64

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

l_list_t* newList();
void printList(l_list_t* list);
bool nodeEqauls(node_t* a, node_t* b);
void removeNode(l_list_t** list, node_t* node);
void addNode(l_list_t** list, node_t* node, double sort_val);
node_t* findNode(l_list_t* list, node_t* node);
node_t* pop(l_list_t** list);
void push(l_list_t** list, node_t* node);
node_t* newNode(int x, int y, double g_score);