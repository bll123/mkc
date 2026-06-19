/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

/* references:
  https://github.com/drifter1/compiler/blob/master/src
*/

%define api.pure full
%define api.prefix {mkcyy}
%define parse.error verbose
%define parse.trace true
%locations

%code requires {
#  include <stdio.h>
#  include <stdint.h>
#  include <stdbool.h>
#  include <stdlib.h>
#  include <string.h>

#  include "mkc_ast.h"
#  include "mkc_def.h"
#  include "mkc_fileop.h"
#  include "mkc_list.h"
#  include "mkc_var.h"

  typedef void *mkcyyscan_t;

#  define MKCYYPARSE_PARAM mkcyyscan_t scanner
#  define MKCYYLEX_PARAM scanner
  /* define YY_BUF_SIZE so that the definition */
  /* from mkc_lex.h is not needed, and a consistent size is ensured */
#  define YY_BUF_SIZE 32768
}

%code {
  /* mkc_lex.h could be included, but it introduces a dependency loop */

  void mkcyyerror (MKCYYLTYPE* mkcyyllocp, mkcyyscan_t unused, mkc_astmain_t *ast, const char* msg);
  int mkcyylex (MKCYYSTYPE* mkcyylvalp, MKCYYLTYPE* mkcyyllocp, mkcyyscan_t scanner);
  typedef struct yy_buffer_state *YY_BUFFER_STATE;
  YY_BUFFER_STATE mkcyy_create_buffer ( FILE *file, int size , mkcyyscan_t yyscanner );
  void mkcyypush_buffer_state ( YY_BUFFER_STATE new_buffer , mkcyyscan_t yyscanner );
  void mkcyypop_buffer_state ( mkcyyscan_t yyscanner );
}

%union {
  char          *sval;
  mkc_astnode_t *astnode;
  mkc_list_t    *list;
  mkc_value_t   *value;
}

%lex-param {void *scanner}
%parse-param {void *scanner} {mkc_astmain_t* ast}

%start mkc

%token T_EQUAL                "="
%token T_LEFT_BRACE           "{"
%token T_LEFT_BRACKET         "["
%token T_LEFT_PAREN           "("
%token T_OP_AND               "&&"
%token T_OP_DIVIDE            "/"
%token T_OP_MINUS             "-"
%token T_OP_MODULO            "%"
%token T_OP_MULTIPLY          "*"
%token T_OP_NOT               "!"
%token T_OP_NUM_EQ            "=="
%token T_OP_NUM_GE            ">="
%token T_OP_NUM_GT            ">"
%token T_OP_NUM_LE            "<="
%token T_OP_NUM_LT            "<"
%token T_OP_NUM_NE            "!="
%token T_OP_OR                "||"
%token T_OP_PLUS              "+"
%token T_OP_RANGE             ".."
%token T_OP_STR_EQ            "eq"
%token T_OP_STR_GE            "ge"
%token T_OP_STR_GT            "gt"
%token T_OP_STR_LE            "le"
%token T_OP_STR_LT            "lt"
%token T_OP_STR_NE            "ne"
%token T_RIGHT_BRACE          "}"
%token T_RIGHT_BRACKET        "]"
%token T_RIGHT_PAREN          ")"
%token T_SEMICOLON            ";"
%token T_VAL_FALSE            "false"
%token <sval> T_VAL_INTEGER   "[0-9]+"
%token T_VAL_TRUE             "true"

/* a T_VARIABLE is a ${...} complex */
%token <sval> T_ID_FILE_NAME
%token <sval> T_ID_PATH_NAME
%token <sval> T_ID_VAR_NAME
%token <sval> T_VAL_STATIC_STRING "'...'"
%token <sval> T_VAL_ENV_VARIABLE  "$ENV{...}"
%token <sval> T_VAL_QUOTED_STRING "..."
%token <sval> T_VARIABLE          "${...}"

// keywords
%token T_STMT_ELSE            "else"
%token T_LOOP_BREAK           "break"
%token T_LOOP_CONTINUE        "continue"
%token T_STMT_FOREACH         "foreach"
%token T_STMT_IF              "if"
%token T_STMT_SET             "set"
%token T_STMT_WHILE           "while"

// directives
%token T_STMT_CONFIGURE       "configure"
%token T_STMT_DEBUG           "mkcdebug"
%token T_STMT_INCLUDE         "include"
%token T_STMT_LOADCACHE       "load_cache"
%token T_STMT_PROJECT         "project"

// commands
%token T_STMT_FUNCTION        "function"
%token T_STMT_OPTION          "option"
%token T_STMT_PRINT           "print"
%token T_STMT_PROFILE         "profile"

%token T_CHK_COMP_FLAG        "check_compile_flag"
%token T_CHK_CONST            "check_const"
%token T_CHK_FUNCTION         "check_function"
%token T_CHK_HEADER           "check_header"
%token T_CHK_LINK_FLAG        "check_link_flag"
%token T_CHK_SIZE             "check_size"
%token T_CHK_STRUCT_MEMBER    "check_struct_member"
%token T_CHK_TYPE             "check_type"

%token T_ADD_COMP_FLAG        "add_compile_flag"
%token T_ADD_LINK_FLAG        "add_link_flag"

// attributes
%token T_ATTR_COMPILER        "compiler"
%token T_ATTR_COMP_FLAGS      "compiler_flags"
%token T_ATTR_DEFINE_ZERO     "define_zero"
%token T_ATTR_HEADER          "header"
%token T_ATTR_INPUT           "input"
%token T_ATTR_LINK_FLAGS      "link_flags"
%token T_ATTR_METHOD          "method"
%token T_ATTR_NAME            "name"
%token T_ATTR_NEGATE          "negate"
%token T_ATTR_OUTPUT          "output"
%token T_ATTR_SOURCE          "source"

%type <astnode> expr
%type <astnode> integer stringvalue basicvalue
%type <astnode> varname
%type <astnode> pathname
%type <astnode> varvalue

%type <astnode> valuelist pathlist
%type <astnode> range

%type <astnode> funcargs

%type <astnode> stmtblock_or_semi stmtblock stmtlist stmt
%type <astnode> directive
// program control
%type <astnode> ifexpr ifstmt elseif elseclause
%type <astnode> foreachstmt whilestmt function loopcontrol
// commands
%type <astnode> printstmt setstmt configurestmt projectstmt loadcachestmt
%type <astnode> includestmt profilestmt
// checks
%type <astnode> checkcommand chkcompflag chklinkflag
%type <astnode> chksize chktype chkstructmember
%type <astnode> chkfunction
// attributes
%type <astnode> attr attrname source header compilerflags linkflags negate
%type <astnode> method input output compiler define_zero

// precedence rules: the lowest precedence comes first
%left T_OP_OR
%left T_OP_AND
%left T_OP_NUM_EQ T_OP_NUM_NE T_OP_STR_EQ T_OP_STR_NE
%left T_OP_NUM_LT T_OP_NUM_LE T_OP_NUM_GT T_OP_NUM_GE T_OP_STR_LT T_OP_STR_LE T_OP_STR_GT T_OP_STR_GE
%left T_OP_MINUS T_OP_PLUS
%left T_OP_MULTIPLY T_OP_DIVIDE T_OP_MODULO
%precedence UNARY
%nonassoc T_OP_RANGE

%%
mkc:
    %empty
  | stmtlist[v]
    {
      mkc_astnode_t   *stmts;

      stmts = mkc_ast_mk_main (ast, $v,
          yylloc.first_line, yylloc.first_column);
      mkc_ast_set_main (ast, stmts);
    }
  ;

stmt[v]:
    T_SEMICOLON
    {
      $v = NULL;
    }
  | ifstmt[a]
    {
      $v = $a;
    }
  | foreachstmt[a]
    {
      $v = $a;
    }
  | whilestmt[a]
    {
      $v = $a;
    }
  | function
    {
      $v = NULL;
    }
  | printstmt[a]
    {
      $v = $a;
    }
  | setstmt[a]
    {
      $v = $a;
    }
  | profilestmt[a]
    {
      $v = $a;
    }
  | directive[a]
    {
      $v = $a;
    }
  | configurestmt[a]
    {
      $v = $a;
    }
  | projectstmt[a]
    {
      $v = $a;
    }
  | loadcachestmt[a]
    {
      $v = $a;
    }
  | includestmt[a]
    {
      $v = $a;
    }
  | checkcommand[a]
    {
      $v = $a;
    }
  | attr[a]
    {
      $v = $a;
    }
  | loopcontrol[a]
    {
      $v = $a;
    }
  ;

attr[v]:
    attrname[a]
    {
      $v = $a;
    }
  | compilerflags[a]
    {
      $v = $a;
    }
  | linkflags[a]
    {
      $v = $a;
    }
  | header[a]
    {
      $v = $a;
    }
  | source[a]
    {
      $v = $a;
    }
  | negate[a]
    {
      $v = $a;
    }
  | define_zero[a]
    {
      $v = $a;
    }
  | method[a]
    {
      $v = $a;
    }
  | input[a]
    {
      $v = $a;
    }
  | output[a]
    {
      $v = $a;
    }
  | compiler[a]
    {
      $v = $a;
    }
  ;

loopcontrol[v]:
    T_LOOP_BREAK T_SEMICOLON
    {
      $v = mkc_ast_mk_loop_control (ast, MKC_T_LOOP_BREAK,
          yylloc.first_line, yylloc.first_column);
    }
  | T_LOOP_CONTINUE T_SEMICOLON
    {
      $v = mkc_ast_mk_loop_control (ast, MKC_T_LOOP_CONTINUE,
          yylloc.first_line, yylloc.first_column);
    }
  ;

checkcommand[v]:
    chkcompflag[a]
    {
      $v = $a;
    }
  | chklinkflag[a]
    {
      $v = $a;
    }
  | chksize[a]
    {
      $v = $a;
    }
  | chktype[a]
    {
      $v = $a;
    }
  | chkstructmember[a]
    {
      $v = $a;
    }
  | chkfunction[a]
    {
      $v = $a;
    }
  ;

/* a statement-block may contain either statements or attribute statements */
stmtblock[v]:
    T_LEFT_BRACE stmtlist[a] T_RIGHT_BRACE
    {
      $v = $a;
    }
  ;

stmtblock_or_semi[v]:
    T_SEMICOLON
    {
      $v = NULL;
    }
  | stmtblock[a]
    {
      $v = $a;
    }
  ;

stmtlist[v]:
    stmt[a]
    {
      $v = mkc_ast_mk_stmtlist (ast, NULL, $a, yylloc.first_line, yylloc.first_column);
    }
  | stmtlist[a] stmt[b]
    {
      $v = mkc_ast_mk_stmtlist (ast, $a, $b, yylloc.first_line, yylloc.first_column);
    }
  ;

/* flow control */

ifstmt[v]:
    ifexpr[a] stmtblock[b] elseclause[c]
    {
      $v = mkc_ast_mk_if (ast, $a, $b, NULL, $c,
          yylloc.first_line, yylloc.first_column);
    }
  | ifexpr[a] stmtblock[b] elseif[c] elseclause[d]
    {
      $v = mkc_ast_mk_if (ast, $a, $b, $c, $d,
          yylloc.first_line, yylloc.first_column);
    }
  ;

ifexpr[v]:
    T_STMT_IF T_LEFT_PAREN expr[a] T_RIGHT_PAREN
    {
      $v = $a;
    }
  ;

elseif[v]:
    T_STMT_ELSE ifexpr[a] stmtblock[b]
    {
      mkc_astnode_t   *elseif;

      elseif = mkc_ast_mk_elseif (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
      $v = mkc_ast_mk_stmtlist (ast, NULL, elseif,
          yylloc.first_line, yylloc.first_column);
    }
  | elseif[a] T_STMT_ELSE ifexpr[b] stmtblock[c]
    {
      mkc_astnode_t   *elseif;

      elseif = mkc_ast_mk_elseif (ast, $b, $c,
          yylloc.first_line, yylloc.first_column);
      $v = mkc_ast_mk_stmtlist (ast, $v, elseif,
          yylloc.first_line, yylloc.first_column);
    }
  ;

elseclause[v]:
    %empty
    {
      $v = NULL;
    }
  | T_STMT_ELSE stmtblock[a]
    {
      $v = $a;
    }
  ;

foreachstmt[v]:
    T_STMT_FOREACH varname[a] valuelist[l] stmtblock[b]
    {
      $v = mkc_ast_mk_foreach (ast, $a, $l, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | T_STMT_FOREACH varname[a] range[b] stmtblock[c]
    {
      $v = mkc_ast_mk_foreach_range (ast, $a, $b, $c,
          yylloc.first_line, yylloc.first_column);
    }
  ;

range[v]:
    varvalue[a] T_OP_RANGE varvalue[b]
    {
      $v = mkc_ast_mk_range (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  ;

whilestmt[v]:
    T_STMT_WHILE T_LEFT_PAREN expr[a] T_RIGHT_PAREN stmtblock[b]
    {
      $v = mkc_ast_mk_while (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  ;

function[v]:
    T_STMT_FUNCTION varname[a] T_LEFT_PAREN funcargs[l] T_RIGHT_PAREN
        stmtblock[b]
    {
      $v = mkc_ast_mk_function (ast, $a, $l, $b,
          yylloc.first_line, yylloc.first_column);
    }
  ;

funcargs:
    varname
    {
    }
  | funcargs varname
    {
    }
  ;

directive[v]:
    T_STMT_DEBUG varname[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_debug (ast, $a, NULL,
          yylloc.first_line, yylloc.first_column);
    }
  | T_STMT_DEBUG varname[a] varname[b] T_SEMICOLON
    {
      $v = mkc_ast_mk_debug (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | T_STMT_OPTION varname[a] varvalue[b] T_SEMICOLON
    {
      $v = NULL;
    }
  ;

printstmt[v]:
    T_STMT_PRINT varvalue[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_print (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

setstmt[v]:
    T_STMT_SET varname[a] expr[b] T_SEMICOLON
    {
      $v = mkc_ast_mk_set (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | T_STMT_SET varname[a] valuelist[b] T_SEMICOLON
    {
      $v = mkc_ast_mk_set (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | T_STMT_SET varname[a] T_LEFT_BRACKET valuelist[l] T_RIGHT_BRACKET T_SEMICOLON
    {
      $v = mkc_ast_mk_set (ast, $a, $l,
          yylloc.first_line, yylloc.first_column);
    }
  ;

profilestmt[v]:
    T_STMT_PROFILE varname[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_profile (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  ;

configurestmt[v]:
    T_STMT_CONFIGURE stmtblock[a]
    {
      $v = mkc_ast_mk_configure (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

projectstmt[v]:
    T_STMT_PROJECT stmtblock[a]
    {
      $v = mkc_ast_mk_project (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

loadcachestmt[v]:
    T_STMT_LOADCACHE integer[a] stmtblock[b]
    {
      $v = mkc_ast_mk_loadcache (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  ;

// ### this should be: pathname...
// ### would need to extract the path from the value.
includestmt[v]:
    T_STMT_INCLUDE T_ID_PATH_NAME[a] T_SEMICOLON
    {
      FILE            *fh;
      YY_BUFFER_STATE tstate;
      int             rc;

      fh = mkc_fopen ($a, "r");
      tstate = mkcyy_create_buffer (fh, YY_BUF_SIZE, scanner);
      mkcyypush_buffer_state (tstate, scanner);
      rc = mkcyyparse (scanner, ast);
      mkcyypop_buffer_state (scanner);
      if (rc != 0) {
        YYABORT;
      }
      $v = NULL;
    }
  ;

chkcompflag[v]:
    T_ADD_COMP_FLAG varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_chk_comp_flag (ast, $a, $b, MKC_ADD,
          yylloc.first_line, yylloc.first_column);
    }
  | T_CHK_COMP_FLAG varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_chk_comp_flag (ast, $a, $b, MKC_CHK,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chklinkflag[v]:
    T_ADD_LINK_FLAG varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_chk_link_flag (ast, $a, $b, MKC_ADD,
          yylloc.first_line, yylloc.first_column);
    }
  | T_CHK_LINK_FLAG varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_chk_link_flag (ast, $a, $b, MKC_CHK,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chksize[v]:
    T_CHK_SIZE varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_chk_size (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chktype[v]:
    T_CHK_TYPE varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_chk_type (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chkstructmember[v]:
    T_CHK_STRUCT_MEMBER varvalue[a] varvalue[b] stmtblock_or_semi[c]
    {
      $v = mkc_ast_mk_chk_struct_member (ast, $a, $b, $c,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chkfunction[v]:
    T_CHK_FUNCTION varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_chk_function (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* a name is the user requested name that overrides the generated name */
attrname[v]:
    T_ATTR_NAME varname[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attr_name (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* negation flag for compiler-flag check */
negate[v]:
    T_ATTR_NEGATE T_SEMICOLON
    {
      $v = mkc_ast_mk_attr_negate (ast,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* define-zero flag for configure */
define_zero[v]:
    T_ATTR_DEFINE_ZERO T_SEMICOLON
    {
      $v = mkc_ast_mk_attr_define_zero (ast,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* method for configure/install/etc. */
method[v]:
    T_ATTR_METHOD varvalue[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attr_method (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

input[v]:
    T_ATTR_INPUT varvalue[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attr_input (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

output[v]:
    T_ATTR_OUTPUT varvalue[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attr_output (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

compiler[v]:
    T_ATTR_COMPILER varvalue[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attr_compiler (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* a list of source files */
source[v]:
    T_ATTR_SOURCE pathlist[l] T_SEMICOLON
    {
      $v = NULL;
    }
  ;

/* a list of header files */
header[v]:
    T_ATTR_HEADER pathlist[l] T_SEMICOLON
    {
      $v = mkc_ast_mk_attr_header (ast, $l,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* a list of compiler flags */
compilerflags[v]:
    T_ATTR_COMP_FLAGS varvalue[a] T_SEMICOLON
    {
      $a = mkc_ast_mk_value_list (ast, NULL, $a,
          yylloc.first_line, yylloc.first_column);
      $v = mkc_ast_mk_attr_compflags (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  | T_ATTR_COMP_FLAGS valuelist[l] T_SEMICOLON
    {
      $v = mkc_ast_mk_attr_compflags (ast, $l,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* a list of link flags */
linkflags[v]:
    T_ATTR_LINK_FLAGS varvalue[a] T_SEMICOLON
    {
      $a = mkc_ast_mk_value_list (ast, NULL, $a,
          yylloc.first_line, yylloc.first_column);
      $v = mkc_ast_mk_attr_linkflags (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  | T_ATTR_LINK_FLAGS valuelist[l] T_SEMICOLON
    {
      $v = mkc_ast_mk_attr_linkflags (ast, $l,
          yylloc.first_line, yylloc.first_column);
    }
  ;

pathlist[v]:
    pathname[a]
    {
      $v = mkc_ast_mk_value_list (ast, NULL, $a,
          yylloc.first_line, yylloc.first_column);
    }
  | pathlist[l] pathname[a]
    {
      $v = mkc_ast_mk_value_list (ast, $l, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

expr[v]:
    varvalue[a]
    {
      $v = $a;
    }
  | expr[a] T_OP_AND expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_AND, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_OR expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_OR, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_NUM_EQ expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_NUM_EQ, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_NUM_NE expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_NUM_NE, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_NUM_LT expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_NUM_LT, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_NUM_LE expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_NUM_LE, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_NUM_GT expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_NUM_GT, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_NUM_GE expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_NUM_GE, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_STR_EQ expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_STR_EQ, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_STR_NE expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_STR_NE, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_STR_LT expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_STR_LT, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_STR_LE expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_STR_LE, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_STR_GT expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_STR_GT, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_STR_GE expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_STR_GE, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_MINUS expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_MINUS, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_PLUS expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_PLUS, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_MULTIPLY expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_MULTIPLY, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_DIVIDE expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_DIVIDE, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_MODULO expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_MODULO, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | T_OP_PLUS expr[a] %prec UNARY
    {
      $v = mkc_ast_mk_unary_op (ast, $a, MKC_T_OP_UNARY_PLUS,
          yylloc.first_line, yylloc.first_column);
    }
  | T_OP_MINUS expr[a] %prec UNARY
    {
      $v = mkc_ast_mk_unary_op (ast, $a, MKC_T_OP_UNARY_MINUS,
          yylloc.first_line, yylloc.first_column);
    }
  | T_OP_NOT expr[a] %prec UNARY
    {
      $v = mkc_ast_mk_unary_op (ast, $a, MKC_T_OP_NOT,
          yylloc.first_line, yylloc.first_column);
    }
  | T_LEFT_PAREN expr[a] T_RIGHT_PAREN
    {
      $v = $a;
    }
  ;

pathname[v]:
    T_ID_FILE_NAME[a]
    {
      $v = mkc_ast_mk_value (ast, MKC_T_ID_FILE_NAME, $a,
          yylloc.first_line, yylloc.first_column);
    }
  | T_ID_PATH_NAME[a]
    {
      $v = mkc_ast_mk_value (ast, MKC_T_ID_PATH_NAME, $a,
          yylloc.first_line, yylloc.first_column);
    }
  | varvalue[a]
    {
      $v = $a;
    }
  ;

varvalue[v]:
    varname[a]
    {
      $v = $a;
    }
  | basicvalue[a]
    {
      $v = $a;
    }
  | T_VARIABLE[a]
    {
      $v = mkc_ast_mk_value (ast, MKC_T_VARIABLE, $a,
          yylloc.first_line, yylloc.first_column);
    }
  | T_VAL_ENV_VARIABLE[a]
    {
      $v = mkc_ast_mk_value (ast, MKC_T_VAL_ENV_VARIABLE, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* defining a list as having two or more values solves */
/* some grammar issues */
/* statements expecting a list will need to check if the value */
/* passed is a list or a single string */
valuelist[v]:
    varvalue[a] varvalue[b]
    {
      $v = mkc_ast_mk_value_list (ast, NULL, $a,
          yylloc.first_line, yylloc.first_column);
      $v = mkc_ast_mk_value_list (ast, $v, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | valuelist[l] varvalue[a]
    {
      $v = mkc_ast_mk_value_list (ast, $l, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

basicvalue[v]:
    stringvalue[a]
    {
      $v = $a;
    }
  | integer[a]
    {
      $v = $a;
    }
  ;

stringvalue[v]:
    T_VAL_QUOTED_STRING[a]
    {
      $v = mkc_ast_mk_value (ast, MKC_T_VAL_QUOTED_STRING, $a,
          yylloc.first_line, yylloc.first_column);
    }
  | T_VAL_STATIC_STRING[a]
    {
      $v = mkc_ast_mk_value (ast, MKC_T_VAL_STATIC_STRING, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

varname[v]:
    T_ID_VAR_NAME[a]
    {
      $v = mkc_ast_mk_value (ast, MKC_T_ID_VAR_NAME, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

integer[v]:
    T_VAL_INTEGER[a]
    {
      $v = mkc_ast_mk_value (ast, MKC_T_VAL_INTEGER, $a,
          yylloc.first_line, yylloc.first_column);
    }
  | T_VAL_TRUE
    {
      $v = mkc_ast_mk_value (ast, MKC_T_VAL_TRUE, NULL,
          yylloc.first_line, yylloc.first_column);
    }
  | T_VAL_FALSE
    {
      $v = mkc_ast_mk_value (ast, MKC_T_VAL_FALSE, NULL,
          yylloc.first_line, yylloc.first_column);
    }
  ;

%%

void
mkcyyerror (MKCYYLTYPE* mkcyyllocp, mkcyyscan_t unused,
    mkc_astmain_t *ast, const char* msg)
{
  fprintf (stderr, "[%d:%d]: %s\n",
      mkcyyllocp->first_line, mkcyyllocp->first_column, msg);
}
