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
%token <str> COMP_NAME
%token <str> COMP_VALUE
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
  COMP COMP_NAME ASSIGN COMP_VALUE { printf("Adding component %s - %s\n", $2, $4);
                                     add_node(&all_nodes, $2, $4);
                                    }
;

relationship:
  BENCH BI COMP_NAME { printf("The benches: Bench launches %s - %s\n", $1, $3);
                       struct node * node_a = get_node(&all_nodes, $3);
                       node_a->in_fd = bench_to_entry_fd;
                       node_a->out_fd = entry_to_bench_fd; }

| BENCH SINGLE COMP_NAME { printf("The benches: Bench launches (single) %s - %s\n", $1, $3);
                           struct node * node_a = get_node(&all_nodes, $3);
                           node_a->in_fd = bench_to_entry_fd; }

| COMP_NAME SINGLE BENCH { printf("The benches: Bench recieves from %s\n", $1);
                           struct node * node_a = get_node(&all_nodes, $1);
                           node_a->out_fd = entry_to_bench_fd; }

| COMP_NAME BI COMP_NAME { printf("bi between %s and %s\n", $1, $3);
                           struct node * node_a = get_node(&all_nodes, $1);
                           struct node * node_b = get_node(&all_nodes, $3);
                           link_node(&all_nodes, node_a, node_b);
                           link_node(&all_nodes, node_a, node_b); }

| COMP_NAME SINGLE COMP_NAME { printf("single from %s to %s\n", $1, $3);
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
  /*
  if (argc >= 3) {
    bench_to_entry_fd = atoi(argv[1]);
    entry_to_bench_fd = atoi(argv[2]);    
  } else {
    printf("Usage: plumber <in_fd> <out_fd>\n");
    printf("\tConfiguration file is: config_plumber.cfg\n");
    return(1);
  }
  */

  yyparse();

  // Need to create and distribute fds correctly
  // Bench is (almost) taken care of

  print_nodes(&all_nodes);

  // Start executing each component
  // TODO: Launch more than one
  //launch_node(all_nodes.current);

  return 0;
}

yyerror(char *s) {
  fprintf(stderr, "error: %s\n", s);
}
