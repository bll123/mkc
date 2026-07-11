%top {
  /*
   * Copyright 2026 Brad Lanam Pleasant Hill CA
   */

#  include <stdio.h>
#  include <stddef.h>
#  include <stdint.h>

#  include "mkc_grammar.h"

  /* no idea if this is the right way to work around this */
#  define YYSTYPE MKCYYSTYPE
#  define YYLTYPE MKCYYLTYPE
}

%option prefix="mkcyy"
%option align
%option ecs
%option noinput nounput noyywrap nodefault
%option reentrant bison-bridge bison-locations
%option yylineno
%option never-interactive

%{
  /* this was copied from somewhere... */
  #define YY_USER_ACTION \
      yylloc->first_line = yylloc->last_line; \
      yylloc->first_column = yylloc->last_column; \
      if (yylloc->last_line == yylineno) \
        yylloc->last_column += yyleng; \
      else { \
        yylloc->last_line = yylineno; \
        yylloc->last_column = yytext + yyleng - strrchr (yytext, '\n'); \
      }
%}

DIGIT           [0-9]

/* base utf8 from https://github.com/sunxfancy/flex-bison-examples/blob/master/utf8/lexer.l */
/* more exact ranges from https://www.w3.org/2005/03/23-lex-U */

ASC       [\x00-\x7f]
ASCN      [\x00-\x09\x0b-\x7f]
U2A       [\xc2-\xdf]
U3A       [\xe0]
U3B       [\xe1-\xec]
U3C       [\xed]
U3D       [\xee-\xef]
U4A       [\xf0]
U4B       [\xf1-\xf3]
U4C       [\xf4]
UR        [\x80-\xbf]
U3AR      [\xa0-\xbf]
U3CR      [\x80-\x9f]
U4AR      [\x90-\xbf]
U4CR      [\x80-\x8f]

U2      {U2A}{UR}
U3      {U3A}{U3AR}{UR}|{U3B}{UR}{UR}|{U3C}{U3CR}{UR}|{U3D}{UR}{UR}
U4      {U4A}{U4AR}{UR}{UR}|{U4B}{UR}{UR}{UR}|{U4C}{U4CR}{UR}{UR}

UANY    {ASC}|{U2}|{U3}|{U4}
UANYN   {ASCN}|{U2}|{U3}|{U4}
UONLY   {U2}|{U3}|{U4}

TXT_PLUS_HYPHEN   [+-]
/* variable names are allowed to have _, +, - */
VAR_ALPHA         [A-Za-z_]
VAR_ALNUM         [0-9A-Za-z_]
VAR_INNER         [0-9A-Za-z${}_]
/* filenames and directory names may include: _ - + . */
/* (for include files) */
PATH_ALNUM        [0-9A-Za-z_./]

/* these ranges also include various unicode space characters */
/* and other characters that may not be great for names */
/* let the users deal with that */
VAR_UGA             {VAR_ALPHA}|{U2}|{U3}|{U4}
VAR_UG              {VAR_ALNUM}|{U2}|{U3}|{U4}
VAR_INNER_UG        {VAR_INNER}|{U2}|{U3}|{U4}
PATH_UG             {PATH_ALNUM}|{U2}|{U3}|{U4}

DOUBLE_QUOTE          ["]
SINGLE_QUOTE          [']
COMMENT               ("//"|"#")[^\r\n]*[\r\n]
WHITESPACE            [ \t\r\n]|{COMMENT}

%%

"=="          { return T_OP_NUM_EQ; }
"!="          { return T_OP_NUM_NE; }
">="          { return T_OP_NUM_GE; }
">"           { return T_OP_NUM_GT; }
"<="          { return T_OP_NUM_LE; }
"<"           { return T_OP_NUM_LT; }
"eq"          { return T_OP_STR_EQ; }
"ge"          { return T_OP_STR_GE; }
"gt"          { return T_OP_STR_GT; }
"le"          { return T_OP_STR_LE; }
"lt"          { return T_OP_STR_LT; }
"ne"          { return T_OP_STR_NE; }
"eq~"         { return T_OP_STR_EQ_REGEX; }
"ne~"         { return T_OP_STR_NE_REGEX; }
"&&"          { return T_OP_AND; }
"||"          { return T_OP_OR; }
"!"           { return T_OP_NOT; }
"/"           { return T_OP_DIVIDE; }
"-"           { return T_OP_MINUS; }
"%"           { return T_OP_MODULO; }
"*"           { return T_OP_MULTIPLY; }
"+"           { return T_OP_PLUS; }
"is_list"     { return T_OP_IS_LIST; }
"is_defined"  { return T_OP_IS_DEFINED; }
"{"           { return T_LEFT_BRACE; }
"["           { return T_LEFT_BRACKET; }
"("           { return T_LEFT_PAREN; }
"}"           { return T_RIGHT_BRACE; }
"]"           { return T_RIGHT_BRACKET; }
")"           { return T_RIGHT_PAREN; }
";"           { return T_SEMICOLON; }

"true"|"TRUE"     { return T_VAL_TRUE; }
"false"|"FALSE"   { return T_VAL_FALSE; }
"bootstrap"       { return T_VAL_BOOTSTRAP; }

"break"     { return T_LOOP_BREAK; }
"continue"  { return T_LOOP_CONTINUE; }
"else"      { return T_STMT_ELSE; }
"exit"      { return T_EXIT; }
"foreach"   { return T_STMT_FOREACH; }
"in"        { return T_IN; }
"function"  { return T_STMT_FUNCTION; }
"if"        { return T_STMT_IF; }
"print"     { return T_STMT_PRINT; }
"profile"   { return T_STMT_PROFILE; }
"set"       { return T_STMT_SET; }
"while"     { return T_STMT_WHILE; }
"range"     { return T_RANGE; }

"check_include_compilation"   { return T_STMT_CHK_INC_COMPILE; }
"check_include_dependencies"  { return T_STMT_CHK_INC_DEPS; }
"check_include_guards"        { return T_STMT_CHK_INC_GUARDS; }
"configure"   { return T_STMT_CONFIGURE; }
"load_cache"  { return T_STMT_LOADCACHE; }
"mark"        { return T_STMT_MARK; }
"include"     { return T_STMT_INCLUDE; }
"mkcdebug"    { return T_STMT_DEBUG; }
"option"      { return T_STMT_OPTION; }
"project"     { return T_STMT_PROJECT; }

"check_arg_count"           { return T_CHK_ARG_COUNT; }
"check_compile_flag"        { return T_CHK_COMP_FLAG; }
"check_constant"            { return T_CHK_CONST; }
"check_define"              { return T_CHK_DEFINE; }
"check_function"            { return T_CHK_FUNCTION; }
"check_header"              { return T_CHK_HEADER; }
"check_link_flag"           { return T_CHK_LINK_FLAG; }
"check_package"             { return T_CHK_PACKAGE; }
"check_size"                { return T_CHK_SIZE; }
"check_struct_member"       { return T_CHK_STRUCT_MEMBER; }
"check_type"                { return T_CHK_TYPE; }
"check_shell_extract"       { return T_CHK_SHELL_EXTRACT; }

"add_compile_flag"          { return T_ADD_COMP_FLAG; }
"add_link_flag"             { return T_ADD_LINK_FLAG; }

"alternate"                 { return T_ATTR_ALTERNATE; }
"compiler"                  { return T_ATTR_COMPILER; }
"compiler_flags"            { return T_ATTR_COMP_FLAGS; }
"context"                   { return T_ATTR_CONTEXT; }
"define_zero"               { return T_ATTR_DEFINE_ZERO; }
"header"                    { return T_ATTR_HEADER; }
"input"                     { return T_ATTR_INPUT; }
"library_version"           { return T_ATTR_LIBRARY_VERSION; }
"link_flags"                { return T_ATTR_LINK_FLAGS; }
"match"                     { return T_ATTR_MATCH; }
"method"                    { return T_ATTR_METHOD; }
"name"                      { return T_ATTR_NAME; }
"negate"                    { return T_ATTR_NEGATE; }
"output"                    { return T_ATTR_OUTPUT; }
"path"                      { return T_ATTR_PATH; }
"replace"                   { return T_ATTR_REPLACE; }
"source"                    { return T_ATTR_SOURCE; }
"version"                   { return T_ATTR_VERSION; }

({DIGIT}+) {
    yylval->sval = strdup (yytext);
    return T_VAL_INTEGER;
  }

({VAR_UGA}({VAR_UG}|{TXT_PLUS_HYPHEN})*) {
    yylval->sval = strdup (yytext);
    return T_ID_VAR_NAME;
  }

({PATH_UG}({PATH_UG}|{TXT_PLUS_HYPHEN})*[./]({PATH_UG}|{TXT_PLUS_HYPHEN})*) {
    /* a path must have a dot or slash in the path */
    /* otherwise a quoted string must be used */
    yylval->sval = strdup (yytext);
    return T_ID_PATH_NAME;
  }

{DOUBLE_QUOTE}(([^"\\\r\n]|\\.)*){DOUBLE_QUOTE} {
    yytext [yyleng - 1] = '\0';
    yyleng -= 2;
    yylval->sval = strdup (yytext + 1);
    return T_VAL_QUOTED_STRING;
  }

{SINGLE_QUOTE}([^'\r\n]*){SINGLE_QUOTE} {
    yytext [yyleng - 1] = '\0';
    yyleng -= 2;
    yylval->sval = strdup (yytext + 1);
    return T_VAL_STATIC_STRING;
  }

({WHITESPACE})+   { }

"${"({VAR_INNER_UG})+"}"  {
    yytext [yyleng - 1] = '\0';
    yyleng -= 3;
    yylval->sval = strdup (yytext + 2);
    return T_VARIABLE;
  }

"$ENV{"({VAR_ALNUM}|"-")+"}"  {
    /* is this definition correct? what characters are valid for env names? */
    yytext [yyleng - 1] = '\0';
    yyleng -= 6;
    yylval->sval = strdup (yytext + 5);
    return T_VAL_ENV_VARIABLE;
  }

. {
    fprintf (stderr, "[%d:%d]: %s %s\n",
        yylloc->first_line, yylloc->first_column,
        "unrecognized symbol", yytext
        );
    exit (0);
  }

<<EOF>> {
    yypop_buffer_state (yyscanner);
    if (! YY_CURRENT_BUFFER) {
      yyterminate ();
    }
  }

%%
