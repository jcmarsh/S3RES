/* read the startup script */
%{
#include "plumbing.h"
#include "pb.tab.h"
%}

%%
[ \t\r]
"Bench"			{ yylval.str = "Bench"; return BENCH; }
"RSMR"                  { yylval.rep_type = RSMR; return P_RSMR; }
"SMR"			{ yylval.rep_type = SMR; return P_SMR; }
"DMR"			{ yylval.rep_type = DMR; return P_DMR; }
"TMR"			{ yylval.rep_type = TMR; return P_TMR; }
"--"			{ return START_PIPE; }
"->"			{ return END_PIPE; }
"("				{ return START_VOTE; }
")"				{ return END_VOTE; }
":="			{ return ASSIGN; }
","				{ return DELIM; }
[a-z][a-zA-Z_]+	{ yylval.str = strdup(yytext); return VAR_NAME; }
[A-Z][a-zA-Z_]+ { yylval.str = strdup(yytext); return NAMED_OB; }
-?[0-9]+		{ yylval.str = strdup(yytext); return NUMBER_VAL; }
"//".*			{ return EOL; }
\n				{ return EOL; }
.				{ printf("Mystery character %c, %s\n", *yytext, yytext); }
%%
