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
      printf("Adding: %s - %s\n", $1, $3);
      printf("TODO: Need to do checking on %s\n", $3);
      add_node(&all_nodes, $1, $3, NONE, NULL); }

  | VAR_NAME ASSIGN rep_comp NAMED_OB { 
      printf("Adding TMR: %s %s %s\n", $1, $3, $4);
      printf("TODO: Checking on: %s and %s\n", $3, $4);
      add_node(&all_nodes, $1, $3, TMR, $4); }
  ;

rep_comp
  : START_VOTE NAMED_OB END_VOTE { $$ = $2; }
  ;

relationship
  : BENCH arrow VAR_NAME {
      struct node * node_a = get_node(&all_nodes, $3);
      node_a->in_fd = bench_to_entry_fd; }

  | VAR_NAME arrow BENCH {
      struct node * node_a = get_node(&all_nodes, $1);
      node_a->out_fd = entry_to_bench_fd; }

  | VAR_NAME arrow VAR_NAME {
      struct node * node_a = get_node(&all_nodes, $1);
      struct node * node_b = get_node(&all_nodes, $3);
      link_node(&all_nodes, node_a, node_b); }
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

  print_nodes(&all_nodes);


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
          int pipe_fds[2];
          if (pipe(pipe_fds) == -1) {
            printf("Plumber pipe error\n");
          } else {
            current->out_fd = pipe_fds[1];
            next_in_line->in_fd = pipe_fds[0];
          }
        }
      }
    }
  }

  // Start executing each component
  launch_node(&all_nodes);

  return 0;
}

yyerror(char *s) {
  fprintf(stderr, "error: %s\n", s);
}
