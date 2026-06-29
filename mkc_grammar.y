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
#  include "mkc_error.h"
#  include "mkc_fileop.h"
#  include "mkc_list.h"
#  include "mkc_parse.h"
// #  include "mkc_var.h"

  typedef void *mkcyyscan_t;

#  define MKCYYPARSE_PARAM mkcyyscan_t scanner
#  define MKCYYLEX_PARAM scanner
  /* define YY_BUF_SIZE so that the definition */
  /* from mkc_lex.h is not needed, and a consistent size is ensured */
#  define YY_BUF_SIZE 32768
}

%code {
  /* mkc_lex.h could be included, but it introduces a dependency loop */

  void mkcyyerror (MKCYYLTYPE* mkcyyllocp, mkcyyscan_t unused, mkc_parse_t *parse, mkc_astmain_t *ast, mkc_error_t *mkcerr, const char* msg);
  int mkcyylex (MKCYYSTYPE* mkcyylvalp, MKCYYLTYPE* mkcyyllocp, mkcyyscan_t yyscanner);
  typedef struct yy_buffer_state *YY_BUFFER_STATE;
  YY_BUFFER_STATE mkcyy_create_buffer ( FILE *file, int size , mkcyyscan_t yyscanner );
  void mkcyypush_buffer_state ( YY_BUFFER_STATE new_buffer , mkcyyscan_t yyscanner );
  void mkcyypop_buffer_state ( mkcyyscan_t yyscanner );
}

%union {
  char          *sval;
  mkc_astnode_t *astnode;
  mkc_list_t    *list;
}

%lex-param {void *scanner}
%parse-param {void *scanner} {mkc_parse_t * parse} {mkc_astmain_t * ast} {mkc_error_t * mkcerr}

%start mkc

%token T_LEFT_BRACE           "{"
%token T_LEFT_BRACKET         "["
%token T_LEFT_PAREN           "("
%token T_OP_AND               "&&"
%token T_OP_DIVIDE            "/"
%token T_OP_IS_DEFINED        "is_defined"
%token T_OP_IS_LIST           "is_list"
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
%token T_OP_STR_EQ_REGEX      "eq~"
%token T_OP_STR_GE            "ge"
%token T_OP_STR_GT            "gt"
%token T_OP_STR_LE            "le"
%token T_OP_STR_LT            "lt"
%token T_OP_STR_NE            "ne"
%token T_OP_STR_NE_REGEX      "ne~"
%token T_RIGHT_BRACE          "}"
%token T_RIGHT_BRACKET        "]"
%token T_RIGHT_PAREN          ")"
%token T_SEMICOLON            ";"
%token T_VAL_FALSE            "false"
%token <sval> T_VAL_INTEGER   "[0-9]+"
%token T_VAL_TRUE             "true"
%token T_VAL_BOOTSTRAP        "bootstrap"

/* a T_VARIABLE is a ${...} complex */
%token <sval> T_ID_PATH_NAME
%token <sval> T_ID_VAR_NAME
%token <sval> T_VAL_STATIC_STRING "'...'"
%token <sval> T_VAL_ENV_VARIABLE  "$ENV{...}"
%token <sval> T_VAL_QUOTED_STRING "..."
%token <sval> T_VARIABLE          "${...}"

// keywords
%token T_EXIT                 "exit"
%token T_LOOP_BREAK           "break"
%token T_LOOP_CONTINUE        "continue"
%token T_STMT_ELSE            "else"
%token T_STMT_FOREACH         "foreach"
%token T_STMT_IF              "if"
%token T_STMT_SET             "set"
%token T_STMT_WHILE           "while"

// directives
%token T_STMT_CONFIGURE       "configure"
%token T_STMT_DEBUG           "mkcdebug"
%token T_STMT_INCLUDE         "include"
%token T_STMT_LOADCACHE       "load_cache"
%token T_STMT_MARK            "mark"
%token T_STMT_PROJECT         "project"

// commands
%token T_STMT_FUNCTION        "function"
%token T_STMT_OPTION          "option"
%token T_STMT_PRINT           "print"
%token T_STMT_PROFILE         "profile"

%token T_CHK_ARG_COUNT        "check_arg_count"
%token T_CHK_COMP_FLAG        "check_compile_flag"
%token T_CHK_CONST            "check_const"
%token T_CHK_DEFINE           "check_define"
%token T_CHK_FUNCTION         "check_function"
%token T_CHK_HEADER           "check_header"
%token T_CHK_LINK_FLAG        "check_link_flag"
%token T_CHK_PACKAGE          "check_package"
%token T_CHK_SIZE             "check_size"
%token T_CHK_STRUCT_MEMBER    "check_struct_member"
%token T_CHK_TYPE             "check_type"
%token T_CHK_SHELL_EXTRACT    "shell_extract"

%token T_ADD_COMP_FLAG        "add_compile_flag"
%token T_ADD_LINK_FLAG        "add_link_flag"

// attributes
%token T_ATTR_COMPILER        "compiler"
%token T_ATTR_COMP_FLAGS      "compiler_flags"
%token T_ATTR_CONTEXT         "context"
%token T_ATTR_DEFINE_ZERO     "define_zero"
%token T_ATTR_HEADER          "header"
%token T_ATTR_INPUT           "input"
%token T_ATTR_LINK_FLAGS      "link_flags"
%token T_ATTR_METHOD          "method"
%token T_ATTR_NAME            "name"
%token T_ATTR_NEGATE          "negate"
%token T_ATTR_OUTPUT          "output"
%token T_ATTR_PATH            "path"
%token T_ATTR_SOURCE          "source"

%type <astnode> expr
%type <astnode> integer stringvalue basicvalue
%type <astnode> varname
%type <astnode> varvalue
%type <astnode> pathname

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
%type <astnode> profilestmt markstmt
// checks
%type <astnode> checkcommand chkcompflag chklinkflag
%type <astnode> chksize chktype chkstructmember chkargcount
%type <astnode> chkfunction chkdefine chkconst chkpackage
%type <astnode> chkshellcmd
// attributes
%type <astnode> attr attrname source header compilerflags linkflags negate
%type <astnode> method input output compiler define_zero context attrpath

// precedence rules: the lowest precedence comes first
%nonassoc T_OP_RANGE
%left T_OP_OR
%left T_OP_AND
%left T_OP_NUM_EQ T_OP_NUM_NE T_OP_STR_EQ T_OP_STR_NE T_OP_STR_EQ_REGEX T_OP_STR_NE_REGEX
%left T_OP_NUM_LT T_OP_NUM_LE T_OP_NUM_GT T_OP_NUM_GE T_OP_STR_LT T_OP_STR_LE T_OP_STR_GT T_OP_STR_GE
%left T_OP_MINUS T_OP_PLUS
%left T_OP_MULTIPLY T_OP_DIVIDE T_OP_MODULO
%precedence UNARY
%nonassoc T_OP_IS_LIST T_OP_IS_DEFINED

%%
mkc:
    %empty
  | stmtlist[a]
    {
      mkc_ast_mk_main (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

stmt[v]:
    T_SEMICOLON
    {
      $v = NULL;
    }
// control statements
  | T_EXIT[a] integer[b] T_SEMICOLON
    {
      $v = mkc_ast_mk_exit (ast, $b,
          yylloc.first_line, yylloc.first_column);
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
// internal statements
  | loadcachestmt[a]
    {
      $v = $a;
    }
// statements
  | configurestmt[a]
    {
      $v = $a;
    }
  | directive[a]
    {
      $v = $a;
    }
  | includestmt
    {
      $v = NULL;
    }
  | markstmt[a]
    {
      $v = $a;
    }
  | printstmt[a]
    {
      $v = $a;
    }
  | profilestmt[a]
    {
      $v = $a;
    }
  | projectstmt[a]
    {
      $v = $a;
    }
  | setstmt[a]
    {
      $v = $a;
    }
// checks
  | checkcommand[a]
    {
      $v = $a;
    }
// attributes and other statement internal to statement blocks
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
  | compiler[a]
    {
      $v = $a;
    }
  | compilerflags[a]
    {
      $v = $a;
    }
  | context[a]
    {
      $v = $a;
    }
  | define_zero[a]
    {
      $v = $a;
    }
  | header[a]
    {
      $v = $a;
    }
  | input[a]
    {
      $v = $a;
    }
  | linkflags[a]
    {
      $v = $a;
    }
  | method[a]
    {
      $v = $a;
    }
  | negate[a]
    {
      $v = $a;
    }
  | output[a]
    {
      $v = $a;
    }
  | attrpath[a]
    {
      $v = $a;
    }
  | source[a]
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
    chkargcount[a]
    {
      $v = $a;
    }
  | chkcompflag[a]
    {
      $v = $a;
    }
  | chkconst[a]
    {
      $v = $a;
    }
  | chkdefine[a]
    {
      $v = $a;
    }
  | chklinkflag[a]
    {
      $v = $a;
    }
  | chkpackage[a]
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
  | chkshellcmd[a]
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

// statements

configurestmt[v]:
    T_STMT_CONFIGURE stmtblock[a]
    {
      $v = mkc_ast_mk_configure (ast, $a,
          yylloc.first_line, yylloc.first_column);
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

includestmt:
    T_STMT_INCLUDE pathname[a] T_SEMICOLON
    {
      char    fn [MKC_PATH_MAX];

      *fn = '\0';
      mkc_ast_process_include (ast, $a, fn, sizeof (fn),
          yylloc.first_line, yylloc.first_column);

      if (*fn) {
        FILE    *fh;

        mkc_parse_set_filename (parse, fn);
        fh = mkc_fopen (fn, "r");
        if (fh != NULL) {
          mkc_parse_start (parse, fh);
        }
      }
    }
  ;

loadcachestmt[v]:
    T_STMT_LOADCACHE integer[a] stmtblock[b]
    {
      $v = mkc_ast_mk_loadcache (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  ;

markstmt[v]:
    T_STMT_MARK varname[a] varname[b] T_SEMICOLON
    {
      $v = mkc_ast_mk_mark (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  ;

printstmt[v]:
    T_STMT_PRINT varvalue[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_print (ast, $a,
          yylloc.first_line, yylloc.first_column);
    }
  ;

profilestmt[v]:
    T_STMT_PROFILE varname[a] stmtblock[b]
    {
      $v = mkc_ast_mk_profile (ast, $a, $b,
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

setstmt[v]:
    T_STMT_SET varname[a] expr[b] T_SEMICOLON
    {
      $v = mkc_ast_mk_set (ast, $a, $b, NULL,
          yylloc.first_line, yylloc.first_column);
    }
  | T_STMT_SET varname[a] expr[b] stmtblock[c]
    {
      $v = mkc_ast_mk_set (ast, $a, $b, $c,
          yylloc.first_line, yylloc.first_column);
    }
  | T_STMT_SET varname[a] valuelist[l] T_SEMICOLON
    {
      $v = mkc_ast_mk_set (ast, $a, $l, NULL,
          yylloc.first_line, yylloc.first_column);
    }
  | T_STMT_SET varname[a] T_LEFT_BRACKET varvalue[b] T_RIGHT_BRACKET T_SEMICOLON
    {
      $b = mkc_ast_mk_value_list (ast, NULL, $b,
          yylloc.first_line, yylloc.first_column);
      $v = mkc_ast_mk_set (ast, $a, $b, NULL,
          yylloc.first_line, yylloc.first_column);
    }
  | T_STMT_SET varname[a] T_LEFT_BRACKET valuelist[l] T_RIGHT_BRACKET T_SEMICOLON
    {
      $v = mkc_ast_mk_set (ast, $a, $l, NULL,
          yylloc.first_line, yylloc.first_column);
    }
  | T_STMT_SET varname[a] T_LEFT_BRACKET varvalue[b] T_RIGHT_BRACKET stmtblock[c]
    {
      $b = mkc_ast_mk_value_list (ast, NULL, $b,
          yylloc.first_line, yylloc.first_column);
      $v = mkc_ast_mk_set (ast, $a, $b, $c,
          yylloc.first_line, yylloc.first_column);
    }
  | T_STMT_SET varname[a] T_LEFT_BRACKET valuelist[l] T_RIGHT_BRACKET stmtblock[c]
    {
      $v = mkc_ast_mk_set (ast, $a, $l, $c,
          yylloc.first_line, yylloc.first_column);
    }
  ;

// checks

chkargcount[v]:
    T_CHK_ARG_COUNT varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_check (ast, $a, $b, MKC_T_CHK_ARG_COUNT,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chkcompflag[v]:
    T_ADD_COMP_FLAG varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_check_flag (ast, $a, $b, MKC_ADD, MKC_T_CHK_COMP_FLAG,
          yylloc.first_line, yylloc.first_column);
    }
  | T_CHK_COMP_FLAG varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_check_flag (ast, $a, $b, MKC_CHK, MKC_T_CHK_COMP_FLAG,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chkconst[v]:
    T_CHK_CONST varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_check (ast, $a, $b, MKC_T_CHK_CONST,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chkdefine[v]:
    T_CHK_DEFINE varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_check (ast, $a, $b, MKC_T_CHK_DEFINE,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chklinkflag[v]:
    T_ADD_LINK_FLAG varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_check_flag (ast, $a, $b, MKC_ADD, MKC_T_CHK_LINK_FLAG,
          yylloc.first_line, yylloc.first_column);
    }
  | T_CHK_LINK_FLAG varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_check_flag (ast, $a, $b, MKC_CHK, MKC_T_CHK_LINK_FLAG,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chkpackage[v]:
    T_CHK_PACKAGE varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_chk_package (ast, $a, $b,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chksize[v]:
    T_CHK_SIZE varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_check (ast, $a, $b, MKC_T_CHK_SIZE,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chktype[v]:
    T_CHK_TYPE varvalue[a] stmtblock_or_semi[b]
    {
      $v = mkc_ast_mk_check (ast, $a, $b, MKC_T_CHK_TYPE,
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
      $v = mkc_ast_mk_check (ast, $a, $b, MKC_T_CHK_FUNCTION,
          yylloc.first_line, yylloc.first_column);
    }
  ;

chkshellcmd[v]:
    T_CHK_SHELL_EXTRACT pathname[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_check (ast, $a, NULL, MKC_T_CHK_SHELL_EXTRACT,
          yylloc.first_line, yylloc.first_column);
    }
  ;

// attributes

compiler[v]:
    T_ATTR_COMPILER varvalue[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attribute (ast, $a, MKC_T_ATTR_COMPILER,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* context for set */
context[v]:
    T_ATTR_CONTEXT varvalue[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attribute (ast, $a, MKC_T_ATTR_CONTEXT,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* define-zero flag for configure */
define_zero[v]:
    T_ATTR_DEFINE_ZERO T_SEMICOLON
    {
      $v = mkc_ast_mk_attribute (ast, NULL, MKC_T_ATTR_DEFINE_ZERO,
          yylloc.first_line, yylloc.first_column);
    }
  ;

input[v]:
    T_ATTR_INPUT varvalue[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attribute (ast, $a, MKC_T_ATTR_INPUT,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* method for configure/install/etc. */
method[v]:
    T_ATTR_METHOD varvalue[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attribute (ast, $a, MKC_T_ATTR_METHOD,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* a name is the user requested name that overrides the generated name */
attrname[v]:
    T_ATTR_NAME varname[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attribute (ast, $a, MKC_T_ATTR_NAME,
          yylloc.first_line, yylloc.first_column);
    }
  ;

/* negation flag for compiler-flag check */
negate[v]:
    T_ATTR_NEGATE T_SEMICOLON
    {
      $v = mkc_ast_mk_attribute (ast, NULL, MKC_T_ATTR_NEGATE,
          yylloc.first_line, yylloc.first_column);
    }
  ;

output[v]:
    T_ATTR_OUTPUT varvalue[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attribute (ast, $a, MKC_T_ATTR_OUTPUT,
          yylloc.first_line, yylloc.first_column);
    }
  ;

attrpath[v]:
    T_ATTR_PATH pathname[a] T_SEMICOLON
    {
      $v = mkc_ast_mk_attribute (ast, $a, MKC_T_ATTR_PATH,
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
  | expr[a] T_OP_STR_EQ_REGEX expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_STR_EQ_REGEX, $b,
          yylloc.first_line, yylloc.first_column);
    }
  | expr[a] T_OP_STR_NE_REGEX expr[b]
    {
      $v = mkc_ast_mk_op (ast, $a, MKC_T_OP_STR_NE_REGEX, $b,
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
  | T_OP_IS_DEFINED T_LEFT_PAREN varname[a] T_RIGHT_PAREN
    {
      $v = mkc_ast_mk_unary_op (ast, $a, MKC_T_OP_IS_DEFINED,
          yylloc.first_line, yylloc.first_column);
    }
  | T_OP_IS_LIST T_LEFT_PAREN varname[a] T_RIGHT_PAREN
    {
      $v = mkc_ast_mk_unary_op (ast, $a, MKC_T_OP_IS_LIST,
          yylloc.first_line, yylloc.first_column);
    }
  ;

pathname[v]:
    T_ID_PATH_NAME[a]
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
  | T_VAL_BOOTSTRAP
    {
      int   bootstrap = MKC_T_VAL_FALSE;

#if MKC_BOOTSTRAP
      bootstrap = MKC_T_VAL_TRUE;
#endif
      $v = mkc_ast_mk_value (ast, bootstrap, NULL,
          yylloc.first_line, yylloc.first_column);
    }
  ;

%%

void
mkcyyerror (MKCYYLTYPE* mkcyyllocp, mkcyyscan_t unused,
    mkc_parse_t *parse, mkc_astmain_t *ast, mkc_error_t *mkcerr,
    const char * msg)
{
  char    tmp [40];

  fprintf (stderr, "%s:", mkc_parse_get_filename (parse));
  mkc_error_line_disp (tmp, sizeof (tmp), mkcyyllocp->first_line, mkcyyllocp->first_column);
  fprintf (stderr, "%s\n", msg);
}

int
mkc_parse (mkc_parse_t *parse, void *scanner,
    mkc_astmain_t *astmain, mkc_error_t *mkcerr)
{
  int     rc;

  rc = mkcyyparse (scanner, parse, astmain, mkcerr);
  if (rc == 1) {
    mkc_error_set (mkcerr, MKC_ERR_PARSE_FAILURE, 0, NULL);
  }
  if (rc == 2) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
  }
  return rc;
}

void
mkc_parse_debug (mkc_parse_t *parse, bool debug)
{
  if (parse == NULL) {
    return;
  }

  mkcyydebug = debug;
}
