/*
 * Useful structs and what not for the plumber program.
 *
 * James Marshall
 */

#include <stdbool.h>
#include "../include/commtypes.h"

typedef enum {NONE, DMR, TMR} replication_t;

// Why 10? No reason.
#define PIPE_LIMIT 10

struct node {
	// Name...
	char* name;
	char* value;
 	
	replication_t rep_strat;
	char* voter_name;

	// comm will need to be changed to multiple named pipes
	int pipe_count;
	struct typed_pipe pipes[PIPE_LIMIT];
};

struct nodelist {
	struct node* current;
	struct nodelist* next;
};

bool add_node(struct nodelist* nodes, char* Name, char* Value, replication_t rep_type, char* voter_name);

struct node* get_node(struct nodelist* nodes, char* Name);

void link_bench(struct node* n, comm_message_t type, int fd_in, int fd_out);
void link_node(comm_message_t type, struct node* fromName, struct node* toName);

void print_nodes(struct nodelist* nodes);

int launch_node(struct nodelist* nodes);
