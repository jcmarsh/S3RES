/*
 * James Marshall 
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "plumbing.h"

// True if succeeds, false otherwise.
bool add_node(struct nodelist* nodes, char* Name, char* Value, replication_t rep_type, char* voter_name) {
	// if current is null, add new node
	if (nodes->current == NULL) {
		struct node *new_node = malloc(sizeof(struct node));
		new_node->name = malloc(strlen(Name));
		new_node->value = malloc(strlen(Value));
		new_node->links = malloc(sizeof(struct nodelist));
		strcpy(new_node->name, Name);
		strcpy(new_node->value, Value);

		// Zero fds
		new_node->in_fd = 0;
		new_node->out_fd = 0;

		// Set replication strategy and voter name (if one)
		new_node->rep_strat = rep_type;
		if (rep_type != NONE) {
			new_node->voter_name = malloc(strlen(voter_name));
			strcpy(new_node->voter_name, voter_name);
		} else {
			new_node->voter_name = NULL;
		}

		nodes->current = new_node;
		nodes->next = malloc(sizeof(struct nodelist));		
		return true;
	} else if (strcmp(Name, nodes->current->name) == 0) {
		// Node already exists, bail
		printf("PLUMBING ERROR: Re-defining component: %s\n", Name);
		return false;
	} else {
		return add_node(nodes->next, Name, Value, rep_type, voter_name);
	}
}

struct node* get_node(struct nodelist* nodes, char* Name) {
	if (nodes->current == NULL) {
		printf("PLUMBING ERROR: get_node, request nodes doesn't exist: %s\n", Name);
		return NULL;
	} else if (strcmp(Name, nodes->current->name) == 0) {
		return nodes->current;
	} else {
		return get_node(nodes->next, Name);
	}
}

// TODO: These links don't really make sense yet...
// Links are going to have to be replaced with pipes of some sort...
void add_link(struct nodelist* froms_nodes, struct node* toNode) {
	if (froms_nodes->current == NULL) {
		froms_nodes->current = toNode;
		froms_nodes->next = malloc(sizeof(struct nodelist));
	} else {
		add_link(froms_nodes->next, toNode);
	}
}

void link_node(struct nodelist* nodes, struct node* fromNode, struct node* toNode) {
	add_link(fromNode->links, toNode);
}

void print_links(struct nodelist* nodes) {
	if (nodes->current == NULL) {
		printf("XXX\n");
	} else {
		printf("%s --> ", nodes->current->name);
		print_links(nodes->next);
	}
}

void print_nodes(struct nodelist* nodes)
{
	if (nodes->current == NULL) {
		printf("XXX\n");
		return;
	} else {
		printf("%s\n\t", nodes->current->name);
		print_links(nodes->current->links);
		print_nodes(nodes->next);
	}
}

// TODO: compare to "forkSingleReplica" in replicas.cpp
int launch_node(struct nodelist* nodes) {
	pid_t currentPID = 0;
	char write_out[3]; // TODO: Handle multiple write out fds
	char read_in[3];
	char** rep_argv;
	// TODO: handle args

	struct node* curr = nodes->current;

	if (curr != NULL) {
		if (curr->rep_strat == NONE) {
			// launch with no replication
			// printf("Launching with no rep: %d -> %s -> %d\n", curr->in_fd, curr->name, curr->out_fd);
			rep_argv = malloc(sizeof(char *) * 4);
			rep_argv[0] = curr->value;
			rep_argv[1] = read_in;
			rep_argv[2] = write_out;
			rep_argv[3] = NULL;
		} else if (curr->rep_strat == DMR) {
			printf("pb.y: No support for DMR\n");
		} else if (curr->rep_strat == TMR) {
			// launch with voter
			// printf("Launching with a voter: %d -> (%s)%s -> %d\n", curr->in_fd, curr->name, curr->voter_name, curr->out_fd);
			rep_argv = malloc(sizeof(char *) * 5);
			rep_argv[0] = curr->voter_name;
			rep_argv[1] = curr->value;
			rep_argv[2] = read_in;
			rep_argv[3] = write_out;
			rep_argv[4] = NULL;
		}

		currentPID = fork();

		if (currentPID >= 0) { // Successful fork
			if (currentPID == 0) { // Child process
				sprintf(read_in, "%02d", curr->in_fd);
				sprintf(write_out, "%02d", curr->out_fd);
				if (-1 == execv(rep_argv[0], rep_argv)) {
					printf("File: %s\n", rep_argv[0]);
					perror("EXEC ERROR!");
					free(rep_argv);
					return -1;
				}
			} else { // Parent Process
				launch_node(nodes->next);
			}
		} else {
			printf("Fork error!\n");
			free(rep_argv);
			return -1;
		}
		free(rep_argv);
	} // curr == NULL, okay.
}
