/*
 * Useful structs and what not for the plumber program.
 *
 * James Marshall
 */

#include <stdbool.h>

struct node {
 	// Name...
 	char* name;
 	char* value;
 	
 	// comm will need to be changed to multiple named pipes
 	int in_fd;
 	int out_fd;

 	// list of nodes it talks to
 	struct nodelist* links;
};

struct nodelist {
	struct node* current;
	struct nodelist* next;
};

bool add_node(struct nodelist* nodes, char* Name, char* Value);

struct node* get_node(struct nodelist* nodes, char* Name);

void link_node(struct nodelist* nodes, struct node* fromName, struct node* toName);

void print_nodes(struct nodelist* nodes);

int launch_node(struct node* launchee);