/*
 * James Marshall 
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "plumbing.h"

// True if succeeds, false otherwise.
// TODO: should eventually allow an arbitrary number of arguments to be passed in. For now, just one for ease
bool add_node(struct nodelist* nodes, char* Name, char* Value, replication_t rep_type, char* voter_name, char* voter_timer, char* priority) {
	// if current is null, add new node
	if (nodes->current == NULL) {
		struct node *new_node = malloc(sizeof(struct node));
		new_node->name = malloc(strlen(Name));
		new_node->value = malloc(strlen(Value));
		strcpy(new_node->name, Name);
		strcpy(new_node->value, Value);

		// Pipes
		new_node->pipe_count = 0;

		// Set replication strategy and voter name (if one)
		new_node->rep_strat = rep_type;
		if (rep_type != NONE) {
			new_node->voter_name = malloc(strlen(voter_name));
			strcpy(new_node->voter_name, voter_name);
			new_node->voter_timer = malloc(strlen(voter_timer));
			strcpy(new_node->voter_timer, voter_timer);
		} else {
			new_node->voter_name = NULL;
			new_node->voter_timer = NULL;
		}
		new_node->priority = malloc(strlen(priority));
		strcpy(new_node->priority, priority);

		nodes->current = new_node;
		nodes->next = malloc(sizeof(struct nodelist));		
		return true;
	} else if (strcmp(Name, nodes->current->name) == 0) {
		// Node already exists, bail
		printf("PLUMBING ERROR: Re-defining component: %s\n", Name);
		return false;
	} else {
		return add_node(nodes->next, Name, Value, rep_type, voter_name, voter_timer, priority);
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

// stupid bench making everything a pain
// Bench already has fds, one passed in should be 0
void link_bench(struct node* n, comm_message_t type, int fd_in, int fd_out, bool timed) {
	n->pipes[n->pipe_count].type = type;
	n->pipes[n->pipe_count].fd_in = fd_in;
	n->pipes[n->pipe_count].fd_out = fd_out;
	n->pipes[n->pipe_count].timed = timed;
	n->pipe_count++;
}

void link_node(comm_message_t type, struct node* fromNode, bool fromTimed, struct node* toNode, bool toTimed) {
	// create pipe
	int pipe_fds[2];
	if (pipe(pipe_fds) == -1) {
		printf("Plumber pipe error\n");
	} else {
		// give half to fromNode
		fromNode->pipes[fromNode->pipe_count].type = type;
		fromNode->pipes[fromNode->pipe_count].fd_in = 0;
		fromNode->pipes[fromNode->pipe_count].fd_out = pipe_fds[1];
		fromNode->pipes[fromNode->pipe_count].timed = fromTimed;
		fromNode->pipe_count++;
		// other half to toNode
		toNode->pipes[toNode->pipe_count].type = type;
		toNode->pipes[toNode->pipe_count].fd_in = pipe_fds[0];
		toNode->pipes[toNode->pipe_count].fd_out = 0;
		toNode->pipes[toNode->pipe_count].timed = toTimed;
		toNode->pipe_count++;
	}
}

void print_nodes(struct nodelist* nodes)
{
	if (nodes->current == NULL) {
		printf("XXX\n");
		return;
	} else {
		printf("%s\n", nodes->current->name);
		print_nodes(nodes->next);
	}
}

char* serializeRep(replication_t rep_type) {
	char* serial;

	if (asprintf(&serial, "%s", REP_TYPE_T[rep_type]) < 0) {
		perror("serializeRep failed");
	}

	return serial;
}

// TODO: compare to "forkSingleReplica" in replicas.cpp
int launch_node(struct nodelist* nodes) {
	pid_t currentPID = 0;

	char** rep_argv;
	// TODO: handle args
	int i;

	struct node* curr = nodes->current;

	if (curr != NULL) {
		int rep_count = 0;
		int other_arg = 0;
		if (curr->rep_strat == NONE) {
			// launch with no replication
			rep_count = 4 + curr->pipe_count;
			rep_argv = malloc(sizeof(char *) * rep_count);
			rep_argv[0] = curr->value;
			rep_argv[1] = curr->priority;
			if (asprintf(&(rep_argv[2]), "%d", curr->pipe_count) < 0) {
				perror("Plumber failed arg pipe_num write");
			}
			other_arg = 3;
			rep_argv[rep_count - 1] = NULL;
		} else {
			// launch with voter // TODO: Add pipe count?
			rep_count = 6 + curr->pipe_count;
			rep_argv = malloc(sizeof(char *) * rep_count);
			rep_argv[0] = curr->voter_name;
			rep_argv[1] = curr->value;
			rep_argv[2] = serializeRep(curr->rep_strat);
			rep_argv[3] = curr->voter_timer;
			rep_argv[4] = curr->priority;
			other_arg = 5;
			rep_argv[rep_count - 1] = NULL;
		}


		for (i = other_arg; i < curr->pipe_count + other_arg; i++) {
			rep_argv[i] = serializePipe(curr->pipes[i - other_arg]);
		}

		currentPID = fork();

		if (currentPID >= 0) { // Successful fork
			if (currentPID == 0) { // Child process
				if (-1 == execv(rep_argv[0], rep_argv)) {
					printf("File: %s\n", rep_argv[0]);
					perror("Plumber: EXEC ERROR!");
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
		// TODO: Need to free all pointers inside too, no?
		free(rep_argv);
	} // curr == NULL, okay.
}
