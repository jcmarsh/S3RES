/* Startup Script */

%{
#include <stdio.h>
#include "plumbing.h"

extern FILE *yyin;

struct nodelist all_nodes;

int bench_to_entry_fd;
int entry_to_bench_fd;

%}

%token BENCH
%token BI
%token SINGLE
%token COMPONENT
%token ARGS
%token EOL

%%
line:
| line relationship EOL { }
| line invocation EOL { }
;

relationship:
  BENCH BI comp { printf("The benches: Bench launches %s\n", $3);
                  struct node * new_node = add_node(&all_nodes, $3);
                  new_node->in_fd = bench_to_entry_fd;
                  new_node->out_fd = entry_to_bench_fd; }

| comp BI comp { printf("bi between %s and %s\n", $1, $3);
                 add_node(&all_nodes, $1);
                 add_node(&all_nodes, $3);
                 link_node(&all_nodes, $1, $3);
                 link_node(&all_nodes, $3, $1); }
| comp SINGLE comp { printf("single between %s and %s\n", $1, $3);
                 add_node(&all_nodes, $1);
                 add_node(&all_nodes, $3);
                 link_node(&all_nodes, $1, $3); }
;

invocation:
  // TODO: add args for each node
  comp ARGS { printf("Set args for: %s with args %s\n", $1, $2); }
;

comp:
  COMPONENT { $$ = $1; }
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
  launch_node(all_nodes.current);

  return 0;
}

yyerror(char *s) {
  fprintf(stderr, "error: %s\n", s);
}
