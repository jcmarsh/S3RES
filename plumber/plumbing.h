/*
 * Useful structs and what not for the plumber program.
 *
 * James Marshall
  */

struct node {
 	// Name...
 	char *name;
 	
 	int in_fd;
 	// list of out fds? For now just one
 	int out_fd;

 	// Needs args

 	// list of nodes it talks to
 	struct nodelist * links;
};

struct nodelist {
	struct node *current;
	struct nodelist *next;
};

struct node* add_node(struct nodelist * nodes, char* Name);

void link_node(struct nodelist * nodes, char* fromName, char* toName);

void print_nodes(struct nodelist * nodes);

int launch_node(struct node * launchee);