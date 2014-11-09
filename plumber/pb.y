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
%token START_PIPE
%token END_PIPE
%token START_VOTE
%token END_VOTE
%token ASSIGN
%token <str> VAR_NAME
%token <str> NAMED_OB
%token EOL
%type <str> arrow
%type <str> rep_comp

%%
line
  :
  | line relationship EOL { }
  | line declaration EOL { }
  | line EOL { }
  ;

declaration
  : VAR_NAME ASSIGN NAMED_OB { 
      // printf("Adding: %s - %s\n", $1, $3);
      add_node(&all_nodes, $1, $3, NONE, NULL); }

  | VAR_NAME ASSIGN rep_comp NAMED_OB { 
      // printf("Adding TMR: %s %s %s\n", $1, $3, $4);
      add_node(&all_nodes, $1, $3, TMR, $4); }
  ;

rep_comp
  : START_VOTE NAMED_OB END_VOTE { $$ = $2; }
  ;

relationship
  : BENCH arrow VAR_NAME {
      struct node * node_a = get_node(&all_nodes, $3);
      link_bench(node_a, commToEnum($2), bench_to_entry_fd, 0); }

  | VAR_NAME arrow BENCH {
      struct node * node_a = get_node(&all_nodes, $1);
      link_bench(node_a, commToEnum($2), 0, entry_to_bench_fd); }

  | VAR_NAME arrow VAR_NAME {
      struct node * node_a = get_node(&all_nodes, $1);
      struct node * node_b = get_node(&all_nodes, $3);
      if (commToEnum($2) == COMM_ERROR) {
        printf("Comm type doesn't exist! %s\n", $2);
        return -1;
      }
      link_node(commToEnum($2), node_a, node_b); }
  ;

arrow
  : START_PIPE NAMED_OB END_PIPE { $$ = $2; }
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

  // print_nodes(&all_nodes);

  // Start executing each component
  launch_node(&all_nodes);

  return 0;
}

yyerror(char *s) {
  fprintf(stderr, "error: %s\n", s);
}
