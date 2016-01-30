/* Startup Script */

%{
#include <stdio.h>
#include "plumbing.h"

extern FILE *yyin;

struct nodelist all_nodes;

struct typed_pipe data_in;
struct typed_pipe cmd_out;

%}

%union {
  char* str;
  replication_t rep_type;
}

%token <str> BENCH
%token <rep_type> P_SMR
%token <rep_type> P_DMR
%token <rep_type> P_TMR
%token START_PIPE
%token END_PIPE
%token START_VOTE
%token END_VOTE
%token ASSIGN
%token DELIM
%token <str> VAR_NAME
%token <str> NAMED_OB
%token <str> NUMBER_VAL
%token EOL
%type <str> arrow
%type <str> rep_comp
%type <str> rep_name
%type <rep_type> rep_strat

%%
line
  :
  | line relationship EOL { }
  | line declaration EOL { }
  | line EOL { }
  ;

declaration
  : VAR_NAME ASSIGN NAMED_OB NUMBER_VAL { 
      add_node(&all_nodes, $1, $3, NONE, NULL, 0, $4); }

  | VAR_NAME ASSIGN NAMED_OB rep_comp rep_strat DELIM NUMBER_VAL NUMBER_VAL {
      add_node(&all_nodes, $1, $4, $5, $3, $7, $8); }
  ;

rep_comp
  : START_VOTE NAMED_OB END_VOTE { $$ = $2; }
  ;

rep_strat
  : P_SMR { $$ = $1; }
  | P_DMR { $$ = $1; }
  | P_TMR { $$ = $1; }
  ;

relationship
  : BENCH arrow VAR_NAME {
      struct node * node_a = get_node(&all_nodes, $3);
      link_bench(node_a, commToEnum($2), data_in.fd_in, 0, false); }

  | BENCH arrow rep_name {
      struct node * node_a = get_node(&all_nodes, $3);
      link_bench(node_a, commToEnum($2), data_in.fd_in, 0, true); }

  | VAR_NAME arrow BENCH {
      struct node * node_a = get_node(&all_nodes, $1);
      link_bench(node_a, commToEnum($2), 0, cmd_out.fd_out, false); }

  | rep_name arrow BENCH {
      struct node * node_a = get_node(&all_nodes, $1);
      link_bench(node_a, commToEnum($2), 0, cmd_out.fd_out, true); }

  | VAR_NAME arrow VAR_NAME {
      struct node * node_a = get_node(&all_nodes, $1);
      struct node * node_b = get_node(&all_nodes, $3);
      if (commToEnum($2) == COMM_ERROR) {
        printf("Comm type doesn't exist! %s\n", $2);
        return -1;
      }
      link_node(commToEnum($2), node_a, false, node_b, false); }
  
  | rep_name arrow rep_name {
      struct node * node_a = get_node(&all_nodes, $1);
      struct node * node_b = get_node(&all_nodes, $3);
      if (commToEnum($2) == COMM_ERROR) {
        printf("Comm type doesn't exist! %s\n", $2);
        return -1;
      }
      link_node(commToEnum($2), node_a, true, node_b, true); }

  | VAR_NAME arrow rep_name {
      struct node * node_a = get_node(&all_nodes, $1);
      struct node * node_b = get_node(&all_nodes, $3);
      if (commToEnum($2) == COMM_ERROR) {
        printf("Comm type doesn't exist! %s\n", $2);
        return -1;
      }
      link_node(commToEnum($2), node_a, false, node_b, true); }

  | rep_name arrow VAR_NAME {
      struct node * node_a = get_node(&all_nodes, $1);
      struct node * node_b = get_node(&all_nodes, $3);
      if (commToEnum($2) == COMM_ERROR) {
        printf("Comm type doesn't exist! %s\n", $2);
        return -1;
      }
      link_node(commToEnum($2), node_a, true, node_b, false); }
  ;

rep_name
  : START_VOTE VAR_NAME END_VOTE { $$ = $2; }
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
  if (argc >= 5) {
    // pipe num is passed in argv[2], but should always be 2
    deserializePipe(argv[3], &data_in);
    deserializePipe(argv[4], &cmd_out);
  } else {
    printf("Usage: plumber <priority> <pipe_num> <RANGE_POSE_DATA:fd_in:0> <MSG_CMD:0:fd_out>\n");
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
