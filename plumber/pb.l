/* read the startup script */
%{
#include "pb.tab.h"
%}

%%
[ \t\r]
"Bench"			{ yylval.str = "Bench"; return BENCH; }
"--"			{ return START_PIPE; }
"->"			{ return END_PIPE; }
"("				{ return START_VOTE; }
")"				{ return END_VOTE; }
":="			{ return ASSIGN; }
","				{ return DELIM; }
[a-z][a-zA-Z_]+	{ yylval.str = strdup(yytext); return VAR_NAME; }
[A-Z][a-zA-Z_]+ { yylval.str = strdup(yytext); return NAMED_OB; }
[0-9]+			{ yylval.str = strdup(yytext); return TIMEOUT_VAL; }
\n				{ return EOL; }
.				{ printf("Mystery character %c, %s\n", *yytext, yytext); }
%%
