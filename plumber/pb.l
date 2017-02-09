/* read the startup script */
%{
#include "plumbing.h"
#include "pb.tab.h"
%}

%%
[ \t\r]
"Bench"			{ yylval.str = "Bench"; return BENCH; }
"SMR"			{ yylval.rep_type = SMR; return RT_SMR; }
"DMR"			{ yylval.rep_type = DMR; return RT_DMR; }
"TMR"			{ yylval.rep_type = TMR; return RT_TMR; }
"ALL"			{ yylval.rep_type = ALL; return RT_ALL; }
"--"			{ return START_PIPE; }
"->"			{ return END_PIPE; }
"("				{ return START_VOTE; }
")"				{ return END_VOTE; }
"["				{ return START_I2; }
"]"				{ return END_I2; }
":="			{ return ASSIGN; }
","				{ return DELIM; }
[a-z][a-zA-Z_]+	{ yylval.str = strdup(yytext); return VAR_NAME; }
[A-Z][a-zA-Z_]+ { yylval.str = strdup(yytext); return NAMED_OB; }
-?[0-9]+		{ yylval.str = strdup(yytext); return NUMBER_VAL; }
"//".*			{ return EOL; }
\n				{ return EOL; }
.				{ printf("Mystery character %c, %s\n", *yytext, yytext); }
%%
