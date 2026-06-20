/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_var.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  MKC_T_ATTR_COMPILER,
  MKC_T_ATTR_COMP_FLAGS,
  MKC_T_ATTR_CONTEXT,
  MKC_T_ATTR_DEFINE_ZERO,
  MKC_T_ATTR_HEADER,
  MKC_T_ATTR_INPUT,
  MKC_T_ATTR_LINK_FLAGS,
  MKC_T_ATTR_METHOD,
  MKC_T_ATTR_NAME,
  MKC_T_ATTR_NEGATE,
  MKC_T_ATTR_OUTPUT,
  MKC_T_ATTR_SOURCE,
  MKC_T_CHK_COMP_FLAG,
  MKC_T_CHK_DEFINE,
  MKC_T_CHK_FUNCTION,
  MKC_T_CHK_LINK_FLAG,
  MKC_T_CHK_SIZE,
  MKC_T_CHK_STRUCT_MEMBER,
  MKC_T_CHK_TYPE,
  MKC_T_ID_FILE_NAME,
  MKC_T_ID_PATH_NAME,
  MKC_T_ID_VAR_NAME,
  MKC_T_LOOP_BREAK,
  MKC_T_LOOP_CONTINUE,
  MKC_T_MAIN,
  MKC_T_OP_AND,
  MKC_T_OP_DIVIDE,
  MKC_T_OP_MINUS,
  MKC_T_OP_MODULO,
  MKC_T_OP_MULTIPLY,
  MKC_T_OP_NOT,
  MKC_T_OP_NUM_EQ,
  MKC_T_OP_NUM_GE,
  MKC_T_OP_NUM_GT,
  MKC_T_OP_NUM_LE,
  MKC_T_OP_NUM_LT,
  MKC_T_OP_NUM_NE,
  MKC_T_OP_OR,
  MKC_T_OP_PLUS,
  MKC_T_OP_STR_EQ,
  MKC_T_OP_STR_GE,
  MKC_T_OP_STR_GT,
  MKC_T_OP_STR_LE,
  MKC_T_OP_STR_LT,
  MKC_T_OP_STR_NE,
  MKC_T_OP_UNARY_MINUS,
  MKC_T_OP_UNARY_PLUS,
  MKC_T_STMTLIST,
  MKC_T_STMT_CONFIGURE,
  MKC_T_STMT_DEBUG,
  MKC_T_STMT_ELSEIF,
  MKC_T_STMT_FOREACH,
  MKC_T_STMT_FUNCTION,
  MKC_T_STMT_IF,
  MKC_T_STMT_LOADCACHE,
  MKC_T_STMT_MARK,
  MKC_T_STMT_PRINT,
  MKC_T_STMT_PROFILE,
  MKC_T_STMT_PROJECT,
  MKC_T_STMT_SET,
  MKC_T_STMT_WHILE,
  MKC_T_VAL_BASIC_STRING,
  MKC_T_VAL_CONSTANT,
  MKC_T_VAL_ENV_VARIABLE,
  MKC_T_VAL_FALSE,
  MKC_T_VAL_INTEGER,
  MKC_T_VAL_LIST,
  MKC_T_VAL_QUOTED_STRING,
  MKC_T_VAL_STATIC_STRING,
  MKC_T_VAL_TRUE,
  MKC_T_VALUE,
  MKC_T_VARIABLE,
  MKC_T_MAX,
} mkc_astnode_token_t;

typedef struct mkc_astnode_t mkc_astnode_t;
typedef struct mkc_astmain_t mkc_astmain_t;

mkc_astnode_t * mkc_ast_mk_value (mkc_astmain_t *astmain, int asttype, char *str, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_value_list (mkc_astmain_t *astmain, mkc_astnode_t *list, mkc_astnode_t *vala, int32_t lineno, int colno);

mkc_astnode_t * mkc_ast_mk_op (mkc_astmain_t *astmain, mkc_astnode_t *vala, int op, mkc_astnode_t *valb, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_range (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *valb, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_unary_op (mkc_astmain_t *astmain, mkc_astnode_t *value, int op, int32_t lineno, int colno);

mkc_astnode_t * mkc_ast_mk_elseif (mkc_astmain_t *astmain, mkc_astnode_t *expr, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_foreach (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *list, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_foreach_range (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *range, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_function (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *arglist, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_if (mkc_astmain_t *astmain, mkc_astnode_t *expr, mkc_astnode_t *stmtblock, mkc_astnode_t *elseif, mkc_astnode_t *elseblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_loop_control (mkc_astmain_t *astmain, mkc_astnode_token_t type, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_main (mkc_astmain_t *astmain, mkc_astnode_t *vala, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_stmtlist (mkc_astmain_t *astmain, mkc_astnode_t *list, mkc_astnode_t *vala, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_while (mkc_astmain_t *astmain, mkc_astnode_t *expr, mkc_astnode_t *stmtblock, int32_t lineno, int colno);

mkc_astnode_t * mkc_ast_mk_configure (mkc_astmain_t *astmain, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_debug (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *valb, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_loadcache (mkc_astmain_t *astmain, mkc_astnode_t *vers, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_mark (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *valb, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_print (mkc_astmain_t *astmain, mkc_astnode_t *vala, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_profile (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_project (mkc_astmain_t *astmain, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_set (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *expr, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_set_value (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *vala, int32_t lineno, int colno);

/* checks */
mkc_astnode_t * mkc_ast_mk_chk_comp_flag (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int addflag, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_chk_define (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_chk_function (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_chk_link_flag (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int addchk, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_chk_size (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_chk_struct_member (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *valb, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_chk_type (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int32_t lineno, int colno);

/* attribute statements */
mkc_astnode_t * mkc_ast_mk_attr_compflags (mkc_astmain_t *astmain, mkc_astnode_t *compflaglist, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_compiler (mkc_astmain_t *astmain, mkc_astnode_t *name, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_context (mkc_astmain_t *astmain, mkc_astnode_t *context, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_define_zero (mkc_astmain_t *astmain, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_header (mkc_astmain_t *astmain, mkc_astnode_t *hdrlist, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_input (mkc_astmain_t *astmain, mkc_astnode_t *name, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_linkflags (mkc_astmain_t *astmain, mkc_astnode_t *linkflaglist, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_method (mkc_astmain_t *astmain, mkc_astnode_t *method, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_name (mkc_astmain_t *astmain, mkc_astnode_t *nm, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_negate (mkc_astmain_t *astmain, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_output (mkc_astmain_t *astmain, mkc_astnode_t *name, int32_t lineno, int colno);

mkc_astmain_t * mkc_ast_init (mkc_log_t *log, const char *dfltprof, const char *comparg, mkc_error_t *mkcerr);
void mkc_ast_set_main (mkc_astmain_t *, mkc_astnode_t *astnode);
int32_t mkc_ast_start (mkc_astmain_t *);
void mkc_ast_free (mkc_astmain_t *astmain);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif
