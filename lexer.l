
%option noyywrap

alpha [a-zA-Z_]
digit [0-9]

%%
[ \t]		;
[ \n]   { yylineno = yylineno + 1;}
"void"	return LexGetTokenInstName("VOID", "");
"int"	return LexGetTokenInstName("INT", "");
"true"	return LexGetTokenInstName("TRUE", "");
"false"	return LexGetTokenInstName("FALSE", "");
"return"	return LexGetTokenInstName("RETURN", "");
{digit}+       return LexGetTokenInstName("NUM", yytext);
{alpha}({alpha}|{digit})* return LexGetTokenInstName("ID", yytext);
"+"     return LexGetTokenInstName("PLUS", "");
"-"     return LexGetTokenInstName("MINUS", "");
"*"     return LexGetTokenInstName("TIMES", "");
"/"     return LexGetTokenInstName("DIVIDE", "");
"%"     return LexGetTokenInstName("MOD", "");
"="     return LexGetTokenInstName("EQUALS", "");
";"     return LexGetTokenInstName("SEMI", "");
"("     return LexGetTokenInstName("LPAREN", "");
")"     return LexGetTokenInstName("RPAREN", "");
"{"     return LexGetTokenInstName("LBRACE", "");
"}"     return LexGetTokenInstName("RBRACE", "");
"<"     return LexGetTokenInstName("LT", "");
">"     return LexGetTokenInstName("GT", "");
","     return LexGetTokenInstName("COMMA", "");
"::"     return LexGetTokenInstName("DBLCOLON", "");
%%
