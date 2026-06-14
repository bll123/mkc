/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>

#include "mkc_ast.h"
#include "mkc_check.h"
#include "mkc_context.h"
#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_list.h"
#include "mkc_os_process.h"
#include "mkc_parse_util.h"
#include "mkc_process.h"
#include "mkc_string.h"
#include "mkc_var.h"

static const char *typenames [MKC_T_MAX] = {
  [MKC_T_ATTR_COMP_FLAGS] = "attr_comp_flags",
  [MKC_T_ATTR_HEADER] = "attr_header",
  [MKC_T_ATTR_LINK_FLAGS] = "attr_link_flags",
  [MKC_T_ATTR_NAME] = "attr_name",
  [MKC_T_ATTR_NEGATE] = "attr_negate",
  [MKC_T_ATTR_SOURCE] = "attr_source",
  [MKC_T_CHK_COMP_FLAG] = "chk_comp_flag",
  [MKC_T_CHK_FUNCTION] = "chk_function",
  [MKC_T_CHK_LINK_FLAG] = "chk_link_flag",
  [MKC_T_CHK_SIZE] = "chk_size",
  [MKC_T_CHK_STRUCT_MEMBER] = "chk_struct_member",
  [MKC_T_CHK_TYPE] = "chk_type",
  [MKC_T_ID_FILE_NAME] = "id_file_name",
  [MKC_T_ID_PATH_NAME] = "id_path_name",
  [MKC_T_ID_VAR_NAME] = "id_var_name",
  [MKC_T_LOOP_BREAK] = "break",
  [MKC_T_LOOP_CONTINUE] = "continue",
  [MKC_T_MAIN] = "main",
  [MKC_T_OP_AND] = "op_and",
  [MKC_T_OP_DIVIDE] = "op_divide",
  [MKC_T_OP_MINUS] = "op_minus",
  [MKC_T_OP_MODULO] = "op_modulo",
  [MKC_T_OP_MULTIPLY] = "op_multiply",
  [MKC_T_OP_NOT] = "op_not",
  [MKC_T_OP_NUM_EQ] = "op_num_eq",
  [MKC_T_OP_NUM_GE] = "op_num_ge",
  [MKC_T_OP_NUM_GT] = "op_num_gt",
  [MKC_T_OP_NUM_LE] = "op_num_le",
  [MKC_T_OP_NUM_LT] = "op_num_lt",
  [MKC_T_OP_NUM_NE] = "op_num_ne",
  [MKC_T_OP_OR] = "op_or",
  [MKC_T_OP_PLUS] = "op_plus",
  [MKC_T_OP_STR_EQ] = "op_str_eq",
  [MKC_T_OP_STR_GE] = "op_str_ge",
  [MKC_T_OP_STR_GT] = "op_str_gt",
  [MKC_T_OP_STR_LE] = "op_str_le",
  [MKC_T_OP_STR_LT] = "op_str_lt",
  [MKC_T_OP_STR_NE] = "op_str_ne",
  [MKC_T_OP_UNARY_MINUS] = "op_unary_minus",
  [MKC_T_OP_UNARY_PLUS] = "op_unary_plus",
  [MKC_T_STMTLIST] = "stmtlist",
  [MKC_T_STMT_DEBUG] = "stmt_debug",
  [MKC_T_STMT_ELSEIF] = "stmt_elseif",
  [MKC_T_STMT_FOREACH] = "stmt_foreach",
  [MKC_T_STMT_FUNCTION] = "stmt_function",
  [MKC_T_STMT_IF] = "stmt_if",
  [MKC_T_STMT_PRINT] = "stmt_print",
  [MKC_T_STMT_SET] = "stmt_set",
  [MKC_T_STMT_PROFILE] = "stmt_profile",
  [MKC_T_STMT_WHILE] = "stmt_while",
  [MKC_T_VAL_ENV_VARIABLE] = "val_env_variable",
  [MKC_T_VAL_FALSE] = "val_false",
  [MKC_T_VAL_INTEGER] = "val_integer",
  [MKC_T_VAL_LIST] = "val_list",
  [MKC_T_VAL_QUOTED_STRING] = "val_quoted_string",
  [MKC_T_VAL_STATIC_STRING] = "val_static_string",
  [MKC_T_VAL_TRUE] = "val_true",
  [MKC_T_VALUE] = "value",
  [MKC_T_VARIABLE] = "variable",
};

typedef struct mkc_ast_op_t {
  mkc_astnode_t       *vala;
  mkc_astnode_t       *valb;
} mkc_ast_op_t;

typedef struct mkc_ast_value_t {
  mkc_value_t       value;
} mkc_ast_value_t;

typedef struct mkc_ast_unary_op_t {
  mkc_astnode_t       *vala;
} mkc_ast_unary_op_t;

typedef struct mkc_ast_list_t {
  mkc_list_t      *list;
} mkc_ast_list_t;

typedef struct mkc_ast_stmtlist_t {
  mkc_list_t      *stmtlist;
} mkc_ast_stmtlist_t;

typedef struct mkc_ast_main_t {
  mkc_astnode_t       *stmtblock;
} mkc_ast_main_t;

typedef struct mkc_ast_print_t {
  mkc_astnode_t       *vala;
} mkc_ast_print_t;

typedef struct mkc_ast_debug_t {
  mkc_astnode_t       *dbga;
  mkc_astnode_t       *dbgb;
} mkc_ast_debug_t;

typedef struct mkc_ast_if_t {
  mkc_astnode_t       *expr;
  mkc_astnode_t       *stmtblock;
  mkc_astnode_t       *elseif;
  mkc_astnode_t       *elseblock;
} mkc_ast_if_t;

typedef struct mkc_ast_elseif_t {
  mkc_astnode_t       *expr;
  mkc_astnode_t       *stmtblock;
} mkc_ast_elseif_t;

typedef struct mkc_ast_foreach_t {
  mkc_astnode_t     *nm;
  mkc_astnode_t     *valuelist;
  mkc_astnode_t     *range;
  mkc_astnode_t     *stmtblock;
} mkc_ast_foreach_t;

typedef struct mkc_ast_while_t {
  mkc_astnode_t     *expr;
  mkc_astnode_t     *stmtblock;
} mkc_ast_while_t;

typedef struct mkc_ast_set_t {
  mkc_astnode_t     *nm;
  mkc_astnode_t     *vala;
} mkc_ast_set_t;

typedef struct mkc_ast_profile_t {
  mkc_astnode_t     *nm;
  mkc_astnode_t     *comp;
  mkc_astnode_t     *stmtblock;
} mkc_ast_profile_t;

typedef struct mkc_ast_chk_comp_flag_t {
  mkc_astnode_t   *vala;
  mkc_astnode_t   *stmtblock;
  int             addchk;
  int             negate;
} mkc_ast_chk_comp_flag_t;

typedef struct mkc_ast_chk_link_flag_t {
  mkc_astnode_t   *vala;
  mkc_astnode_t   *stmtblock;
  int             addchk;
} mkc_ast_chk_link_flag_t;

typedef struct mkc_ast_chk_size_t {
  mkc_astnode_t     *vala;
  mkc_astnode_t     *stmtblock;
} mkc_ast_chk_size_t;

typedef struct mkc_ast_chk_type_t {
  mkc_astnode_t     *vala;
  mkc_astnode_t     *stmtblock;
} mkc_ast_chk_type_t;

typedef struct mkc_ast_chk_struct_member_t {
  mkc_astnode_t     *vala;
  mkc_astnode_t     *valb;
  mkc_astnode_t     *stmtblock;
} mkc_ast_chk_struct_member_t;

typedef struct mkc_ast_chk_function_t {
  mkc_astnode_t     *vala;
  mkc_astnode_t     *stmtblock;
} mkc_ast_chk_function_t;

typedef struct mkc_ast_attr_name_t {
  mkc_astnode_t     *nm;
} mkc_ast_attr_name_t;

typedef struct mkc_ast_attr_header_t {
  mkc_astnode_t     *hdrlist;
} mkc_ast_attr_header_t;

typedef struct mkc_ast_attr_compflag_t {
  mkc_astnode_t     *compflaglist;
} mkc_ast_attr_compflag_t;

typedef struct mkc_ast_attr_linkflag_t {
  mkc_astnode_t     *linkflaglist;
} mkc_ast_attr_linkflag_t;

typedef struct mkc_astnode_t {
  union {
    mkc_ast_attr_compflag_t     compflagattr;
    mkc_ast_attr_linkflag_t     linkflagattr;
    mkc_ast_attr_header_t       hdrattr;
    mkc_ast_attr_name_t         nameattr;
    mkc_ast_chk_comp_flag_t     chkcompflag;
    mkc_ast_chk_link_flag_t     chklinkflag;
    mkc_ast_chk_size_t          chksize;
    mkc_ast_chk_type_t          chktype;
    mkc_ast_chk_struct_member_t chkstructmember;
    mkc_ast_chk_function_t      chkfunction;
    mkc_ast_debug_t             debugstmt;
    mkc_ast_elseif_t            elseif;
    mkc_ast_foreach_t           foreachstmt;
    mkc_ast_while_t             whilestmt;
    mkc_ast_if_t                ifstmt;
    mkc_ast_list_t              list;
    mkc_ast_main_t              main;
    mkc_ast_op_t                op;
    mkc_ast_print_t             printstmt;
    mkc_ast_profile_t           profilestmt;
    mkc_ast_set_t               setstmt;
    mkc_ast_stmtlist_t          stmtlist;
    mkc_ast_unary_op_t          unary_op;
    mkc_ast_value_t             value;
  };
  int32_t               nodenum;
  int32_t               lineno;
  int                   colno;
  int                   asttype;
} mkc_astnode_t;

typedef struct mkc_astmain_t {
  mkc_astnode_t         * stmtblock;
  mkc_value_t           value;
  mkc_profile_t         * profiles;
  mkc_process_t         * process;
  mkc_context_t         * context;
  mkc_astnode_t         ** nodelist;
  mkc_error_t           * mkcerr;
  mkc_log_t             * log;
  int32_t               allocsz;
  int32_t               sz;
  int32_t               ccidx;
  int                   rdepth;
  int                   depth;
  bool                  maxrdepth;
} mkc_astmain_t;

static int32_t  mkcnodenum = 0;

static int32_t mkc_ast_process (mkc_astmain_t *, mkc_astnode_t *astnode, int32_t *ifcond, int32_t *loopcond, int depth);
static mkc_astnode_t * mkc_astnode_init (mkc_astmain_t *astmain, int type, int32_t lineno, int colno);
static void mkc_astnode_free (mkc_astmain_t *astmain, mkc_astnode_t *astnode);
static mkc_value_t *mkc_ast_get_value (mkc_astmain_t *astmain, mkc_astnode_t *astnode);

mkc_astmain_t *
mkc_ast_init (mkc_log_t *log, const char *dfltprof, mkc_error_t *mkcerr)
{
  mkc_astmain_t   *astmain;

  astmain = malloc (sizeof (mkc_astmain_t));
  if (astmain == NULL) {
    return NULL;
  }
  memset (astmain, 0, sizeof (mkc_astmain_t));

  astmain->profiles = mkc_profile_init (log, mkcerr, dfltprof);
  if (astmain->profiles == NULL) {
    mkc_ast_free (astmain);
    return NULL;
  }

  astmain->process = mkc_process_init (astmain->profiles, log, mkcerr);
  if (astmain->process == NULL) {
    mkc_ast_free (astmain);
    return NULL;
  }

  astmain->context = mkc_context_init (mkcerr);
  if (astmain->context == NULL) {
    mkc_ast_free (astmain);
    return NULL;
  }

  astmain->sz = 0;
  astmain->allocsz = 0;
  astmain->nodelist = NULL;

  astmain->mkcerr = mkcerr;
  astmain->log = log;
  astmain->stmtblock = NULL;
  astmain->value.sval = NULL;
  astmain->value.vtype = MKC_VT_INVALID;
  astmain->depth = 0;
  astmain->rdepth = 0;
  astmain->maxrdepth = 0;

  return astmain;
}

int32_t
mkc_ast_start (mkc_astmain_t *astmain)
{
  int32_t     ifcond = 0;
  int32_t     loopcond = true;

  astmain->rdepth = 0;
  mkc_ast_process (astmain, astmain->stmtblock, &ifcond, &loopcond, 0);
  mkc_process_save_cache (astmain->process);
  return mkc_error_value (astmain->mkcerr);
}

void
mkc_ast_free (mkc_astmain_t *astmain)
{
  if (astmain == NULL) {
    return;
  }

  if (astmain->nodelist != NULL) {
    for (int32_t i = 0; i < astmain->sz; ++i) {
      mkc_astnode_free (astmain, astmain->nodelist [i]);
    }
    free (astmain->nodelist);
  }
  if (astmain->process != NULL) {
    mkc_process_free (astmain->process);
  }
  if (astmain->profiles != NULL) {
    mkc_profile_free (astmain->profiles);
  }
  free (astmain);
}

void
mkc_ast_set_fromcache (mkc_astmain_t *astmain, bool flag)
{
  if (astmain == NULL) {
    return;
  }

  mkc_process_set_fromcache (astmain->process, flag);
}

void
mkc_ast_set_main (mkc_astmain_t *astmain, mkc_astnode_t *stmtblock)
{
  astmain->stmtblock = stmtblock;
}

/* for basic values, numbers, strings, variables */
mkc_astnode_t *
mkc_ast_mk_value (mkc_astmain_t *astmain, int asttype, char *str, int32_t lineno, int colno)
{
  mkc_astnode_t    *astnode = NULL;
  mkc_ast_value_t  *astvalue;
  mkc_value_t      *value;

  astnode = mkc_astnode_init (astmain, MKC_T_VALUE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astvalue = &astnode->value;
  value = &astvalue->value;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk-value: %s %s\n", typenames [asttype], str);

  switch (asttype) {
    case MKC_T_VAL_INTEGER: {
      value->ival = atol (str);
      value->vtype = MKC_VT_INTEGER;
      free (str);
      break;
    }
    case MKC_T_VAL_TRUE: {
      value->ival = true;
      value->vtype = MKC_VT_INTEGER;
      break;
    }
    case MKC_T_VAL_FALSE: {
      value->ival = false;
      value->vtype = MKC_VT_INTEGER;
      break;
    }
    case MKC_T_VAL_QUOTED_STRING: {
      value->sval = str;
      value->vtype = MKC_VT_QUOTED_STRING;
      break;
    }
    case MKC_T_ID_FILE_NAME:
    case MKC_T_ID_PATH_NAME:
    case MKC_T_ID_VAR_NAME:
    case MKC_T_VAL_STATIC_STRING: {
      value->sval = str;
      value->vtype = MKC_VT_STATIC_STRING;
      break;
    }
    case MKC_T_VAL_ENV_VARIABLE: {
      value->sval = str;
      value->vtype = MKC_VT_ENV_VARIABLE;
      break;
    }
    case MKC_T_VARIABLE: {
      value->sval = str;
      value->vtype = MKC_VT_VARIABLE;
      break;
    }
  }

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_value_list (mkc_astmain_t *astmain,
    mkc_astnode_t *listnode, mkc_astnode_t *vala, int32_t lineno, int colno)
{
  mkc_list_t      *tlist = NULL;
  mkc_listidx_t   loc;
  mkc_ast_value_t *astvalue;
  mkc_value_t     *value;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: value-list\n");

  if (listnode == NULL) {
    mkc_astnode_t     *astnode;

    astnode = mkc_astnode_init (astmain, MKC_T_VALUE, lineno, colno);
    tlist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, astmain->mkcerr);
    astvalue = &astnode->value;
    value = &astvalue->value;
    value->list = tlist;
    value->vtype = MKC_VT_LIST;
    listnode = astnode;
  }
  astvalue = &listnode->value;
  value = &astvalue->value;
  tlist = value->list;

  value = &vala->value.value;
  mkc_list_set (tlist, value, sizeof (mkc_value_t), &loc);

  return listnode;
}

mkc_astnode_t *
mkc_ast_mk_stmtlist (mkc_astmain_t *astmain,
    mkc_astnode_t *stmtlist, mkc_astnode_t *vala,
    int32_t lineno, int colno)
{
  mkc_astnode_t     *astnode = NULL;
  mkc_list_t    *tlist = NULL;
  mkc_listidx_t loc;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: stmt-list\n");

  if (stmtlist == NULL) {
    astnode = mkc_astnode_init (astmain, MKC_T_STMTLIST, lineno, colno);
    if (astnode == NULL) {
      return NULL;
    }
    astnode->stmtlist.stmtlist = mkc_list_init (MKC_LIST_UNSORTED,
        NULL, NULL, astmain->mkcerr);
    stmtlist = astnode;
  }
  tlist = stmtlist->stmtlist.stmtlist;
  /* the node is already created, there's no need to store the */
  /* entire structure, just store the pointer */
  mkc_list_set (tlist, &vala, sizeof (mkc_astnode_t *), &loc);

  return stmtlist;
}

mkc_astnode_t *
mkc_ast_mk_op (mkc_astmain_t *astmain,
      mkc_astnode_t *vala, int op, mkc_astnode_t *valb,
      int32_t lineno, int colno)
{
  mkc_astnode_t  *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: op\n");

  astnode = mkc_astnode_init (astmain, op, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->op.vala = vala;
  astnode->op.valb = valb;

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_unary_op (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, int op,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: unary-op\n");

  astnode = mkc_astnode_init (astmain, op, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->unary_op.vala = vala;

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_range (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *valb, int32_t lineno, int colno)
{
  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: range\n");
  return NULL;
}

mkc_astnode_t *
mkc_ast_mk_print (mkc_astmain_t *astmain,
    mkc_astnode_t *vala,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: print\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_PRINT, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->printstmt.vala = vala;

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_debug (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, mkc_astnode_t *valb,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: mkcdebug\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_DEBUG, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->debugstmt.dbga = vala;
  astnode->debugstmt.dbgb = valb;

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_set (mkc_astmain_t *astmain,
    mkc_astnode_t *nm, mkc_astnode_t *vala,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: set\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_SET, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->setstmt.nm = nm;
  astnode->setstmt.vala = vala;

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_profile (mkc_astmain_t *astmain,
    mkc_astnode_t *nm, mkc_astnode_t *comp, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: profile\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_PROFILE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->profilestmt.nm = nm;
  astnode->profilestmt.comp = comp;
  astnode->profilestmt.stmtblock = stmtblock;

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_if (mkc_astmain_t *astmain,
    mkc_astnode_t *expr, mkc_astnode_t *stmtblock,
    mkc_astnode_t *elseif, mkc_astnode_t *elseblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: if\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_IF, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->ifstmt.expr = expr;
  astnode->ifstmt.stmtblock = stmtblock;
  /* the elseif is a 'stmtlist' of elseif statements */
  astnode->ifstmt.elseif = elseif;
  astnode->ifstmt.elseblock = elseblock;

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_elseif (mkc_astmain_t *astmain,
    mkc_astnode_t *expr, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: else-if\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_ELSEIF, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->elseif.expr = expr;
  astnode->elseif.stmtblock = stmtblock;

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_else (mkc_astmain_t *astmain,
    mkc_astnode_t *ifstmt, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: else\n");

  ifstmt->ifstmt.elseblock = stmtblock;
  return ifstmt;
}

mkc_astnode_t *
mkc_ast_mk_foreach (mkc_astmain_t *astmain,
    mkc_astnode_t *nm, mkc_astnode_t *list, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: foreach\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_FOREACH, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->foreachstmt.nm = nm;
  astnode->foreachstmt.range = NULL;
  astnode->foreachstmt.valuelist = list;
  astnode->foreachstmt.stmtblock = stmtblock;

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_foreach_range (mkc_astmain_t *astmain,
    mkc_astnode_t *nm, mkc_astnode_t *range, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: foreach-range\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_FOREACH, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->foreachstmt.nm = nm;
  astnode->foreachstmt.range = range;
  astnode->foreachstmt.valuelist = NULL;
  astnode->foreachstmt.stmtblock = stmtblock;

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_while (mkc_astmain_t *astmain,
    mkc_astnode_t *expr, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: while\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_WHILE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->whilestmt.expr = expr;
  astnode->whilestmt.stmtblock = stmtblock;

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_loop_control (mkc_astmain_t *astmain, mkc_astnode_token_t asttype,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: loop-control %s\n", typenames [asttype]);

  astnode = mkc_astnode_init (astmain, asttype, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_function (mkc_astmain_t *astmain,
    mkc_astnode_t *nm, mkc_astnode_t *arglist, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: function\n");

  return NULL;
}

mkc_astnode_t *
mkc_ast_mk_chk_comp_flag (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int addchk, int negate,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: chk-comp-flag\n");

  astnode = mkc_astnode_init (astmain, MKC_T_CHK_COMP_FLAG, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->chkcompflag.vala = vala;
  astnode->chkcompflag.stmtblock = stmtblock;
  astnode->chkcompflag.addchk = addchk;
  astnode->chkcompflag.negate = negate;
  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_chk_link_flag (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int addchk,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: chk-link-flag\n");

  astnode = mkc_astnode_init (astmain, MKC_T_CHK_LINK_FLAG, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->chklinkflag.vala = vala;
  astnode->chklinkflag.stmtblock = stmtblock;
  astnode->chklinkflag.addchk = addchk;
  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_chk_size (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: chk-size\n");

  astnode = mkc_astnode_init (astmain, MKC_T_CHK_SIZE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->chksize.vala = vala;
  astnode->chksize.stmtblock = stmtblock;
  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_chk_type (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: chk-type\n");

  astnode = mkc_astnode_init (astmain, MKC_T_CHK_TYPE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->chktype.vala = vala;
  astnode->chktype.stmtblock = stmtblock;
  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_chk_struct_member (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, mkc_astnode_t *valb, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: chk-struct-member\n");

  astnode = mkc_astnode_init (astmain, MKC_T_CHK_STRUCT_MEMBER, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->chkstructmember.vala = vala;
  astnode->chkstructmember.valb = valb;
  astnode->chkstructmember.stmtblock = stmtblock;
  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_chk_function (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: chk-function\n");

  astnode = mkc_astnode_init (astmain, MKC_T_CHK_FUNCTION, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->chkfunction.vala = vala;
  astnode->chkfunction.stmtblock = stmtblock;
  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_attr_name (mkc_astmain_t *astmain,
    mkc_astnode_t *nm,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: attr-name\n");

  astnode = mkc_astnode_init (astmain, MKC_T_ATTR_NAME, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->nameattr.nm = nm;
  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_attr_negate (mkc_astmain_t *astmain,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: attr-negate\n");

  astnode = mkc_astnode_init (astmain, MKC_T_ATTR_NEGATE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_attr_header (mkc_astmain_t *astmain,
    mkc_astnode_t *hdrlist,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: attr-header\n");

  astnode = mkc_astnode_init (astmain, MKC_T_ATTR_HEADER, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->hdrattr.hdrlist = hdrlist;
  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_attr_compflags (mkc_astmain_t *astmain,
    mkc_astnode_t *compflaglist,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: attr-comp-flags\n");

  astnode = mkc_astnode_init (astmain, MKC_T_ATTR_COMP_FLAGS, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->compflagattr.compflaglist = compflaglist;
  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_attr_linkflags (mkc_astmain_t *astmain,
    mkc_astnode_t *linkflaglist,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: attr-link-flags\n");

  astnode = mkc_astnode_init (astmain, MKC_T_ATTR_LINK_FLAGS, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->linkflagattr.linkflaglist = linkflaglist;
  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_main (mkc_astmain_t *astmain,
    mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: main\n");

  astnode = mkc_astnode_init (astmain, MKC_T_MAIN, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->main.stmtblock = stmtblock;

  return astnode;
}

/* internal routines */

static int32_t
mkc_ast_process (mkc_astmain_t *astmain, mkc_astnode_t *astnode,
    int32_t *ifcond, int32_t *loopcond, int depth)
{
  if (astmain == NULL) {
    return MKC_ERR_FAILURE;
  }

  astmain->depth = depth;
  astmain->rdepth += 1;
  if (astmain->rdepth > astmain->maxrdepth) {
    astmain->maxrdepth = astmain->rdepth;
  }

  if (mkc_error_chk_err (astmain->mkcerr)) {
    mkc_log (astmain->log, MKC_LOG_AST_PROCESS,
        "ast-proc: error: %d\n", mkc_error_value (astmain->mkcerr));
    /* an error occurred, stop processing */
    return mkc_error_value (astmain->mkcerr);
  }

  if (astnode == NULL) {
    mkc_log (astmain->log, MKC_LOG_AST_PROCESS, "ast-proc: null\n");
    astmain->value.ival = 0;
    astmain->value.vtype = MKC_VT_INTEGER;
    return astmain->value.ival;
  }

  mkc_error_set_line_col (astmain->mkcerr, astnode->lineno, astnode->colno);

  mkc_log_loc (astmain->log, MKC_LOG_AST_PROCESS, astnode->lineno, astnode->colno,
      "%*sast-proc: %s\n", astmain->depth * 2, " ", typenames [astnode->asttype]);

  switch (astnode->asttype) {
    case MKC_T_MAIN: {
      mkc_ast_process (astmain, astnode->main.stmtblock, ifcond, loopcond, depth);
      break;
    }

    case MKC_T_VALUE: {
      mkc_value_t   *value;

      value = mkc_ast_get_value (astmain, astnode);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      {
        char  tbuff [MKC_PATH_MAX];

        mkc_log_loc (astmain->log, MKC_LOG_AST,
            astnode->lineno, astnode->colno,
            "%*s%s\n", astmain->depth * 2, " ",
            mkc_value_to_str (value, tbuff, sizeof (tbuff)));
      }

      memcpy (&astmain->value, value, sizeof (mkc_value_t));
      break;
    }

    case MKC_T_STMTLIST: {
      mkc_listidx_t   iteridx;
      mkc_listidx_t   lidx;

      mkc_list_iter_start (astnode->stmtlist.stmtlist, &iteridx);
      while ((lidx = mkc_list_iter_next (astnode->stmtlist.stmtlist, &iteridx)) != MKC_ITER_FINISH) {
        mkc_astnode_t   **listnode;

        if (mkc_error_chk_err (astmain->mkcerr)) {
          break;
        }

        if (mkc_context_check (astmain->context, MKC_CONTEXT_LOOP)) {
          if (*loopcond == false) {
            break;
          }
        }

        listnode = mkc_list_get_by_idx (astnode->stmtlist.stmtlist, lidx);
        if (listnode != NULL && *listnode != NULL) {
          if ((*listnode)->asttype == MKC_T_LOOP_CONTINUE ||
              (*listnode)->asttype == MKC_T_LOOP_BREAK) {
            if (! mkc_context_check (astmain->context, MKC_CONTEXT_LOOP)) {
              mkc_error_set (astmain->mkcerr, MKC_ERR_STMT_NOT_ALLOWED);
              break;
            }

            if ((*listnode)->asttype == MKC_T_LOOP_BREAK) {
              *loopcond = false;
            }

            /* in both cases, the rest of the statement block */
            /* is not executed */
            break;
          }

          mkc_ast_process (astmain, *listnode, ifcond, loopcond, depth);
        }
      }
      break;
    }

    case MKC_T_STMT_PRINT: {
      mkc_value_t   *value;

      value = mkc_ast_get_value (astmain, astnode->printstmt.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_stmt_print (astmain->process, value, 0);
      break;
    }

    case MKC_T_STMT_DEBUG: {
      mkc_value_t   *vala;
      mkc_value_t   *valb = NULL;

      vala = mkc_ast_get_value (astmain, astnode->debugstmt.dbga);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      /* valb can be null */
      valb = mkc_ast_get_value (astmain, astnode->debugstmt.dbgb);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_stmt_debug (astmain->process, vala, valb);
      break;
    }

    case MKC_T_STMT_IF: {
      int32_t   rval;

      mkc_ast_process (astmain, astnode->ifstmt.expr, ifcond, loopcond, depth);
      rval = mkc_process_condition (astmain->process, &astmain->value);
      if (rval) {
        mkc_ast_process (astmain, astnode->ifstmt.stmtblock, &rval, loopcond, depth + 1);
      }
      if (! rval && astnode->ifstmt.elseif != NULL) {
        mkc_ast_process (astmain, astnode->ifstmt.elseif, &rval, loopcond, depth);
      }
      if (! rval && astnode->ifstmt.elseblock != NULL) {
        mkc_ast_process (astmain, astnode->ifstmt.elseblock, &rval, loopcond, depth + 1);
      }
      break;
    }

    case MKC_T_STMT_ELSEIF: {
      int32_t     rval;

      if (*ifcond) {
        /* if the starting if condition is true */
        /* no else-if statements will be processed */
        break;
      }

      mkc_ast_process (astmain, astnode->elseif.expr, ifcond, loopcond, depth);
      rval = mkc_process_condition (astmain->process, &astmain->value);
      if (rval) {
        mkc_ast_process (astmain, astnode->elseif.stmtblock, &rval, loopcond, depth + 1);
        /* if an else-if succeeds, need to pass the results back */
        /* so that other else-if and else blocks are properly processed */
        *ifcond = rval;
      }
      break;
    }

    case MKC_T_STMT_FOREACH: {
#if 0
      mkc_value_t   *value;
      mkc_listidx_t iteridx;
      mkc_listidx_t lidx;
      mkc_profidx_t plocalidx;

      value = mkc_ast_get_value (astmain, astnode->foreachstmt.nm);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      value = mkc_ast_get_value (astmain, astnode->foreachstmt.valuelist);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      plocalidx = mkc_profile_local_create (astmain->profiles);

      mkc_list_iter_start (tlist, &iteridx);
      while ((lidx = mkc_list_iter_next (tlist, &iteridx)) != MKC_ITER_FINISH) {
        mkc_astnode_t   *listnode;
        mkc_value_t *tval;

        if (mkc_error_chk_err (astmain->mkcerr)) {
          break;
        }

        listnode = mkc_list_get_by_idx (tlist, lidx);
        tval = mkc_ast_get_value (astmain, listnode);
        if (mkc_error_chk_err (astmain->mkcerr)) {
          break;
        }
        mkc_process_local_set (astmain->process, nm,
            tval->sval, plocalidx);
        mkc_ast_process (astmain, astnode->foreachstmt.stmtblock, ifcond, loopcond, depth + 1);
      }
#endif
      break;
    }

    case MKC_T_STMT_WHILE: {
      int32_t   rval = true;
      int32_t   count = 0;
      int32_t   limit = 10000;

      limit = mkc_process_get_while_limit (astmain->process);
      mkc_ast_process (astmain, astnode->whilestmt.expr, ifcond, &rval, depth);
      rval = mkc_process_condition (astmain->process, &astmain->value);
      mkc_context_push (astmain->context, MKC_CONTEXT_LOOP, astmain->mkcerr);
      while (rval && count < limit) {
        mkc_ast_process (astmain, astnode->whilestmt.stmtblock, ifcond, &rval, depth + 1);
        if (! rval) {
          /* a break statement was executed */
          break;
        }
        mkc_ast_process (astmain, astnode->whilestmt.expr, ifcond, &rval, depth);
        rval = mkc_process_condition (astmain->process, &astmain->value);
        ++count;
      }
      mkc_context_pop (astmain->context);
      if (count >= limit) {
        mkc_error_set (astmain->mkcerr, MKC_ERR_WHILE_LIMIT_EXCEEDED);
      }
      break;
    }

    case MKC_T_STMT_SET: {
      mkc_value_t *valnm;

      valnm = mkc_ast_get_value (astmain, astnode->setstmt.nm);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      mkc_ast_process (astmain, astnode->setstmt.vala, ifcond, loopcond, depth);
      mkc_process_stmt_set (astmain->process, valnm, &astmain->value);

      break;
    }

    case MKC_T_STMT_PROFILE: {
      mkc_value_t   *valnm;
      mkc_value_t   *valcomp = NULL;

      valnm = mkc_ast_get_value (astmain, astnode->profilestmt.nm);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      if (astnode->profilestmt.comp != NULL) {
        valcomp = mkc_ast_get_value (astmain, astnode->profilestmt.comp);
        if (mkc_error_chk_err (astmain->mkcerr)) {
          break;
        }
      }

      if (! mkc_process_profile_is_current (astmain->process, valnm)) {
        break;
      }

      if (astnode->profilestmt.stmtblock == NULL) {
        mkc_process_stmt_profile (astmain->process, valnm, valcomp);
      } else {
        mkc_profidx_t   pidx;

        mkc_profile_push (astmain->profiles);
        mkc_process_stmt_profile (astmain->process, valnm, valcomp);
        mkc_ast_process (astmain, astnode->profilestmt.stmtblock, ifcond, loopcond, depth + 1);
        pidx = mkc_profile_pop (astmain->profiles);
        mkc_profile_set_active (astmain->profiles, pidx);
      }
      break;
    }

    case MKC_T_ATTR_NAME: {
      mkc_value_t   *valnm;

      if (! mkc_context_check (astmain->context, MKC_CONTEXT_CHECK)) {
        mkc_error_set (astmain->mkcerr, MKC_ERR_STMT_NOT_ALLOWED);
        break;
      }

      valnm = mkc_ast_get_value (astmain, astnode->nameattr.nm);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_attr_name (astmain->process, valnm);
      break;
    }

    case MKC_T_ATTR_HEADER: {
      mkc_value_t   *val;

      if (! mkc_context_check (astmain->context, MKC_CONTEXT_CHECK)) {
        mkc_error_set (astmain->mkcerr, MKC_ERR_STMT_NOT_ALLOWED);
        break;
      }

      val = mkc_ast_get_value (astmain, astnode->hdrattr.hdrlist);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_attr_header (astmain->process, val);
      break;
    }

    case MKC_T_ATTR_COMP_FLAGS: {
      mkc_value_t   *val;

      if (! mkc_context_check (astmain->context, MKC_CONTEXT_CHECK)) {
        mkc_error_set (astmain->mkcerr, MKC_ERR_STMT_NOT_ALLOWED);
        break;
      }

      val = mkc_ast_get_value (astmain, astnode->compflagattr.compflaglist);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_attr_comp_flags (astmain->process, val);
      break;
    }

    case MKC_T_ATTR_LINK_FLAGS: {
      mkc_value_t   *val;

      if (! mkc_context_check (astmain->context, MKC_CONTEXT_CHECK)) {
        mkc_error_set (astmain->mkcerr, MKC_ERR_STMT_NOT_ALLOWED);
        break;
      }

      val = mkc_ast_get_value (astmain, astnode->linkflagattr.linkflaglist);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_attr_link_flags (astmain->process, val);
      break;
    }

    case MKC_T_CHK_COMP_FLAG: {
      mkc_value_t   *val;

      if (astnode->chkcompflag.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_CHECK, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->chkcompflag.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }

      val = mkc_ast_get_value (astmain, astnode->chkcompflag.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      astmain->value.ival = mkc_process_chk_compiler_flag (astmain->process,
          val, astnode->chkcompflag.addchk);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_CHK_LINK_FLAG: {
      mkc_value_t   *val;

      if (astnode->chklinkflag.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_CHECK, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->chklinkflag.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }
      val = mkc_ast_get_value (astmain, astnode->chklinkflag.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      astmain->value.ival = mkc_process_chk_link_flag (astmain->process,
          val, astnode->chklinkflag.addchk);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_CHK_SIZE: {
      mkc_value_t   *val;

      if (astnode->chksize.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_CHECK, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->chksize.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }
      val = mkc_ast_get_value (astmain, astnode->chksize.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      astmain->value.ival = mkc_process_chk_size (astmain->process, val);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_CHK_TYPE: {
      mkc_value_t   *val;

      if (astnode->chktype.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_CHECK, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->chktype.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }
      val = mkc_ast_get_value (astmain, astnode->chktype.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      astmain->value.ival = mkc_process_chk_type (astmain->process, val);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_CHK_STRUCT_MEMBER: {
      mkc_value_t   *vala;
      mkc_value_t   *valb;

      if (astnode->chkstructmember.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_CHECK, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->chkstructmember.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }

      vala = mkc_ast_get_value (astmain, astnode->chkstructmember.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      valb = mkc_ast_get_value (astmain, astnode->chkstructmember.valb);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      astmain->value.ival = mkc_process_chk_struct_member (astmain->process,
          vala, valb);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_CHK_FUNCTION: {
      mkc_value_t   *vala;

      if (astnode->chkfunction.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_CHECK, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->chkfunction.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }

      vala = mkc_ast_get_value (astmain, astnode->chkfunction.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      astmain->value.ival = mkc_process_chk_function (astmain->process, vala);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_OP_AND: {
      int32_t       vala, valb;

      /* && can be short circuited */
      vala = mkc_ast_process (astmain, astnode->op.vala, ifcond, loopcond, depth);
      if (! vala) {
        astmain->value.ival = vala;
        astmain->value.vtype = MKC_VT_INTEGER;
        break;
      }
      valb = mkc_ast_process (astmain, astnode->op.valb, ifcond, loopcond, depth);
      astmain->value.ival = valb;
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_OP_OR: {
      int32_t       vala, valb;

      /* || can be short circuited */
      vala = mkc_ast_process (astmain, astnode->op.vala, ifcond, loopcond, depth);
      if (vala) {
        astmain->value.ival = vala;
        astmain->value.vtype = MKC_VT_INTEGER;
        break;
      }
      valb = mkc_ast_process (astmain, astnode->op.valb, ifcond, loopcond, depth);
      astmain->value.ival = valb;
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_OP_NUM_EQ:
    case MKC_T_OP_NUM_GE:
    case MKC_T_OP_NUM_GT:
    case MKC_T_OP_NUM_LE:
    case MKC_T_OP_NUM_LT:
    case MKC_T_OP_NUM_NE:
    case MKC_T_OP_DIVIDE:
    case MKC_T_OP_MINUS:
    case MKC_T_OP_MODULO:
    case MKC_T_OP_MULTIPLY:
    case MKC_T_OP_PLUS: {
      mkc_value_t vala, valb;

      mkc_ast_process (astmain, astnode->op.vala, ifcond, loopcond, astmain->depth);
      memcpy (&vala, &astmain->value, sizeof (mkc_value_t));
      mkc_ast_process (astmain, astnode->op.valb, ifcond, loopcond, astmain->depth);
      memcpy (&valb, &astmain->value, sizeof (mkc_value_t));

      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      astmain->value.ival = mkc_process_num_op (astmain->process,
          astnode->asttype, &vala, &valb);
      astmain->value.vtype = MKC_VT_INTEGER;
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      break;
    }

    case MKC_T_OP_STR_EQ:
    case MKC_T_OP_STR_NE:
    case MKC_T_OP_STR_LT:
    case MKC_T_OP_STR_LE:
    case MKC_T_OP_STR_GT:
    case MKC_T_OP_STR_GE: {
      mkc_value_t   stra, strb;

      mkc_ast_process (astmain, astnode->op.vala, ifcond, loopcond, astmain->depth);
      memcpy (&stra, &astmain->value, sizeof (mkc_value_t));
      mkc_ast_process (astmain, astnode->op.valb, ifcond, loopcond, astmain->depth);
      memcpy (&strb, &astmain->value, sizeof (mkc_value_t));

      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      astmain->value.ival = mkc_process_str_op (astmain->process,
          astnode->asttype, &stra, &strb);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_OP_NOT:
    case MKC_T_OP_UNARY_MINUS:
    case MKC_T_OP_UNARY_PLUS: {
      mkc_ast_process (astmain, astnode->unary_op.vala, ifcond, loopcond, depth);
      if (astmain->value.vtype != MKC_VT_INTEGER) {
        mkc_error_set (astmain->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT);
        break;
      }
      astmain->value.ival = mkc_process_unary_op (astmain->process, astnode->asttype, &astmain->value);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    default: {
      mkc_error_set (astmain->mkcerr, MKC_ERR_UNHANDLED_VALUE);
      fprintf (stderr, "unhandled value %s\n", typenames [astnode->asttype]);
      break;
    }
  }

  astmain->rdepth -= 1;
  if (astmain->rdepth == 0) {
    mkc_log (astmain->log, MKC_LOG_STATISTICS,
        "-- max-recursion: %d\n", astmain->maxrdepth);
    return mkc_error_value (astmain->mkcerr);
  }

  return astmain->value.ival;
}

static mkc_astnode_t *
mkc_astnode_init (mkc_astmain_t *astmain, int type, int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  if (astmain->sz >= astmain->allocsz) {
    astmain->allocsz += 10;
    astmain->nodelist = realloc (astmain->nodelist,
        sizeof (mkc_astnode_t *) * astmain->allocsz);
    if (astmain->nodelist == NULL) {
      return NULL;
    }
    for (int32_t i = astmain->sz; i < astmain->allocsz; ++i) {
      astmain->nodelist [i] = NULL;
    }
  }

  astnode = malloc (sizeof (mkc_astnode_t));
  if (astnode == NULL) {
    return NULL;
  }
  astmain->nodelist [astmain->sz] = astnode;
  astmain->sz += 1;

  memset (astnode, 0, sizeof (mkc_astnode_t));
  astnode->asttype = type;
  astnode->lineno = lineno;
  astnode->colno = colno;
  astnode->nodenum = mkcnodenum;
  mkcnodenum += 1;

  return astnode;
}

static void
mkc_astnode_free (mkc_astmain_t *astmain, mkc_astnode_t *astnode)
{
  if (astnode == NULL) {
    return;
  }

  switch (astnode->asttype) {
    case MKC_T_VALUE: {
      mkc_value_t *value;

      value = mkc_ast_get_value (astmain, astnode);
      if (value != NULL) {
        mkc_value_free (value);
      }
      break;
    }
    case MKC_T_STMTLIST: {
      mkc_list_free (astnode->stmtlist.stmtlist);
      break;
    }
  }

  free (astnode);
}

static mkc_value_t *
mkc_ast_get_value (mkc_astmain_t *astmain, mkc_astnode_t *astnode)
{
  mkc_value_t   *value = NULL;

  if (astnode == NULL) {
    return NULL;
  }

  if (astnode->asttype != MKC_T_VALUE) {
    mkc_error_set (astmain->mkcerr, MKC_ERR_PARSE_FAILURE);
    return NULL;
  }
  if (astnode->asttype == MKC_T_VALUE) {
    value = &astnode->value.value;
  }

  return value;
}
