/* read the startup script */
%{
#include "pb.tab.h"
%}

%%
[ \t\r]
"Bench"			{ yylval.str = "Bench"; return BENCH; }
"comp"			{ return COMP; }
"<->"			{ return BI; }
"-->"			{ return SINGLE; }
":="			{ return ASSIGN; }
[a-zA-Z_]+		{ yylval.str = strdup(yytext); return COMP_N; }
\"[\(a-zA-Z_]+	{ int len = strlen(yytext) - 2;
				  yylval.str = malloc(len + 1);
				  memcpy(yylval.str, &(yytext[2]), len);
				  yylval.str[len] = NULL; return COMP_V_N; }
\)[a-zA-Z_]+\"	{ int len = strlen(yytext) - 2;
				  yylval.str = malloc(len + 1);
				  memcpy(yylval.str, &(yytext[1]), len);
				  yylval.str[len] = NULL; return COMP_V_VOTER; }
\"[a-zA-Z_]+\"	{ int len = strlen(yytext) - 2;
				  yylval.str = malloc(len + 1);
				  memcpy(yylval.str, &(yytext[1]), len);
				  yylval.str[len] = NULL; return COMP_V_N; }
\n				{ return EOL; }
.				{ printf("Mystery character %c, %s\n", *yytext, yytext); }
%%
