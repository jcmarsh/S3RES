/*
 * Useful structs and what not for the plumber program.
 *
 * James Marshall
  */

struct node {
 	// Name...
 	char *name;
 	// in fd? don't think so
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
