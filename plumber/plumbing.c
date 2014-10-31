/*
 * James Marshall 
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "plumbing.h"

// True if succeeds, false otherwise.
bool add_node(struct nodelist* nodes, char* Name, char* Value) {
	// if current is null, add new node
	if (nodes->current == NULL) {
		printf("Adding node %s - %s\n", Name, Value);
		struct node *new_node = malloc(sizeof(struct node));
		new_node->name = malloc(strlen(Name));
		new_node->value = malloc(strlen(Value));
		new_node->links = malloc(sizeof(struct nodelist));
		strcpy(new_node->name, Name);
		strcpy(new_node->value, Value);
		nodes->current = new_node;
		nodes->next = malloc(sizeof(struct nodelist));
		return true;
	} else if (strcmp(Name, nodes->current->name) == 0) {
		// Node already exists, bail
		printf("PLUMBING ERROR: Re-defining component: %s\n", Name);
		return false;
	} else {
		return add_node(nodes->next, Name, Value);
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
int launch_node(struct node* launchee) {
	pid_t currentPID = 0;
	char write_out[3]; // TODO: Handle multiple write out fds
	char read_in[3];
	// TODO: handle args
	char *rep_argv[] = {launchee->name, read_in, write_out, NULL};

	currentPID = fork();

	if (currentPID >= 0) { // Successful fork
		if (currentPID == 0) { // Child process
			sprintf(read_in, "%02d", launchee->in_fd);
			rep_argv[1] = read_in;
			sprintf(write_out, "%02d", launchee->out_fd);
			rep_argv[2] = write_out;
			if (-1 == execv(rep_argv[0], rep_argv)) {
				perror("EXEC ERROR!");
				return -1;
			}
		} else { // Parent Process
			
		}
	} else {
		printf("Fork error!\n");
		return -1;
	}
}
