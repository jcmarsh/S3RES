/*
 * James Marshall
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "plumbing.h"

struct node* add_node(struct nodelist * nodes, char* Name) {
	// if current is null, add new node
	if (nodes->current == NULL) {
		struct node *new_node = malloc(sizeof(struct node));
		new_node->name = malloc(strlen(Name));
		new_node->links = malloc(sizeof(struct nodelist));
		strcpy(new_node->name, Name);
		nodes->current = new_node;
		nodes->next = malloc(sizeof(struct nodelist));
		return nodes->current;
	} else if (strcmp(Name, nodes->current->name) == 0) {
		// Node already exists, bail
		return nodes->current;
	} else {
		return add_node(nodes->next, Name);
	}
}

void add_link(struct nodelist * froms_nodes, struct node * toNode) {
	if (froms_nodes->current == NULL) {
		froms_nodes->current = toNode;
		froms_nodes->next = malloc(sizeof(struct nodelist));
	} else {
		add_link(froms_nodes->next, toNode);
	}
}

void link_node(struct nodelist * nodes, char* fromName, char* toName) {
	struct node * fromNode = add_node(nodes, fromName);
	struct node * toNode = add_node(nodes, toName);

	add_link(fromNode->links, toNode);
}

void print_links(struct nodelist * nodes) {
	if (nodes->current == NULL) {
		printf("XXX\n");
	} else {
		printf("%s --> ", nodes->current->name);
		print_links(nodes->next);
	}
}

void print_nodes(struct nodelist * nodes)
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
