/* Startup Script */

%{
#include <stdio.h>
#include "plumbing.h"

extern FILE *yyin;

struct nodelist all_nodes;

int bench_to_entry_fd;
int entry_to_bench_fd;

%}

%union {
  char* str;
}

%token <str> BENCH
%token COMP
%token <str> COMP_N
%token <str> COMP_V_N
%token <str> COMP_V_VOTER
%token ASSIGN
%token BI
%token SINGLE
%token EOL

%%
line:
| line relationship EOL { }
| line declaration EOL { }
| line EOL { }
;

declaration:
  COMP COMP_N ASSIGN COMP_V_N { //printf("Adding component %s - %s\n", $2, $4);
                                add_node(&all_nodes, $2, $4, NONE, NULL); }

| COMP COMP_N ASSIGN COMP_V_N COMP_V_VOTER { //printf("Adding comp %s - %s with voter %s\n", $2, $4, $5);
                                             add_node(&all_nodes, $2, $4, TMR, $5); }
;

relationship:
  BENCH BI COMP_N { //printf("The benches: Bench launches %s - %s\n", $1, $3);
                    struct node * node_a = get_node(&all_nodes, $3);
                    node_a->in_fd = bench_to_entry_fd;
                    node_a->out_fd = entry_to_bench_fd; }

| BENCH SINGLE COMP_N { //printf("The benches: Bench launches (single) %s - %s\n", $1, $3);
                        struct node * node_a = get_node(&all_nodes, $3);
                        node_a->in_fd = bench_to_entry_fd; }

| COMP_N SINGLE BENCH { //printf("The benches: Bench recieves from %s\n", $1);
                        struct node * node_a = get_node(&all_nodes, $1);
                        node_a->out_fd = entry_to_bench_fd; }

| COMP_N BI COMP_N { //printf("bi between %s and %s\n", $1, $3);
                     struct node * node_a = get_node(&all_nodes, $1);
                     struct node * node_b = get_node(&all_nodes, $3);
                     link_node(&all_nodes, node_a, node_b);
                     link_node(&all_nodes, node_b, node_a); }

| COMP_N SINGLE COMP_N { //printf("single from %s to %s\n", $1, $3);
                         struct node * node_a = get_node(&all_nodes, $1);
                         struct node * node_b = get_node(&all_nodes, $3);
                         link_node(&all_nodes, node_a, node_b); }
;

%%
int main(int argc, char **argv) {

  // config file is hard coded.
  if (!(yyin = fopen("config_plumber.cfg", "r"))) {
    perror("config_plumber.cfg");
    return(1);
  }

  // fds are passed in as arguments
  if (argc >= 3) {
    bench_to_entry_fd = atoi(argv[1]);
    entry_to_bench_fd = atoi(argv[2]);    
  } else {
    printf("Usage: plumber <in_fd> <out_fd>\n");
    printf("\tConfiguration file is: config_plumber.cfg\n");
    return(1);
  }

  yyparse();

  // Need to create and distribute fds correctly
  // Bench is (almost) taken care of

  print_nodes(&all_nodes);

  // Start executing each component
  // TODO: Launch more than one
  //launch_node(all_nodes.current);

  struct nodelist* next = &all_nodes;
  while(next->current != NULL) {
    struct node* current = next->current;
    next = next->next;

    // Just handling a loop of components for now
    if (current->out_fd == 0) {
      if (current->links->current != NULL) {
        struct node* next_in_line = current->links->current;
        if (next_in_line->in_fd == 0) {
          // Create pipe
          printf("Creating pipe for %s and %s\n", current->name, next_in_line->name);
          int pipe_fds[2];
          if (pipe(pipe_fds) == -1) {
            printf("Plumber pipe error\n");
          } else {
            current->out_fd = pipe_fds[0];
            next_in_line->in_fd = pipe_fds[1];
          }
        }
      }
    }

    if (current->rep_strat == NONE) {
      // launch with no replication
      printf("Launching with no rep: %d -> %s -> %d\n", current->in_fd, current->name, current->out_fd);
    } else if (current->rep_strat == DMR) {
      printf("pb.y: No support for DMR\n");
    } else if (current->rep_strat == TMR) {
      // launch with voter
      printf("Launching with a voter: %d -> (%s)%s -> %d\n", current->in_fd, current->name, current->voter_name, current->out_fd);
    }

  }

  return 0;
}

yyerror(char *s) {
  fprintf(stderr, "error: %s\n", s);
}
