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
#include "mkc_nodiscard.h"
#include "mkc_option.h"
#include "mkc_os_process.h"
#include "mkc_process.h"
#include "mkc_string.h"
#include "mkc_var.h"

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
  mkc_list_t          *list;
} mkc_ast_list_t;

typedef struct mkc_ast_stmtlist_t {
  mkc_list_t          *stmtlist;
} mkc_ast_stmtlist_t;

typedef struct mkc_ast_main_t {
  mkc_astnode_t       *stmtlist;
} mkc_ast_main_t;

/* statements */

typedef struct mkc_ast_stmt_chk_inc_deps_t {
  mkc_astnode_t       *stmtblock;
} mkc_ast_stmt_chk_inc_deps_t;

typedef struct mkc_ast_conf_t {
  mkc_astnode_t       *stmtblock;
  bool                definezero;
} mkc_ast_conf_t;

typedef struct mkc_ast_debug_t {
  mkc_astnode_t       *dbga;
  mkc_astnode_t       *dbgb;
} mkc_ast_debug_t;

typedef struct mkc_ast_elseif_t {
  mkc_astnode_t       *expr;
  mkc_astnode_t       *stmtblock;
} mkc_ast_elseif_t;

typedef struct mkc_ast_exit_t {
  mkc_astnode_t       *vala;
} mkc_ast_exit_t;

typedef struct mkc_ast_foreach_t {
  mkc_astnode_t     *nm;
  mkc_astnode_t     *valuelist;
  mkc_astnode_t     *range;
  mkc_astnode_t     *stmtblock;
} mkc_ast_foreach_t;

typedef struct mkc_ast_range_t {
  mkc_astnode_t     *beg;
  mkc_astnode_t     *end;
  mkc_astnode_t     *incr;
  mkc_value_t       value;
  bool              init;
} mkc_ast_range_t;

typedef struct mkc_ast_function_t {
  mkc_astnode_t     *nm;
  mkc_astnode_t     *argnames;
  mkc_astnode_t     *stmtblock;
} mkc_ast_function_t;

typedef struct mkc_ast_function_call_t {
  mkc_astnode_t     *nm;
  mkc_astnode_t     *funcargs;
} mkc_ast_function_call_t;

typedef struct mkc_ast_if_t {
  mkc_astnode_t       *expr;
  mkc_astnode_t       *stmtblock;
  mkc_astnode_t       *elseif;
  mkc_astnode_t       *elseblock;
} mkc_ast_if_t;

typedef struct mkc_ast_loadcache_t {
  mkc_astnode_t       *version;
  mkc_astnode_t       *stmtblock;
} mkc_ast_loadcache_t;

typedef struct mkc_ast_mark_t {
  mkc_astnode_t       *vala;
  mkc_astnode_t       *valb;
} mkc_ast_mark_t;

typedef struct mkc_ast_print_t {
  mkc_astnode_t       *vala;
} mkc_ast_print_t;

typedef struct mkc_ast_profile_t {
  mkc_astnode_t     *nm;
  mkc_astnode_t     *stmtblock;
} mkc_ast_profile_t;

typedef struct mkc_ast_project_t {
  mkc_astnode_t       *stmtblock;
} mkc_ast_project_t;

typedef struct mkc_ast_set_t {
  mkc_astnode_t     *nm;
  mkc_astnode_t     *vala;
  mkc_astnode_t     *stmtblock;
} mkc_ast_set_t;

typedef struct mkc_ast_while_t {
  mkc_astnode_t     *expr;
  mkc_astnode_t     *stmtblock;
} mkc_ast_while_t;

/* checks */

typedef struct mkc_ast_check_t {
  mkc_astnode_t     *vala;
  mkc_astnode_t     *stmtblock;
} mkc_ast_check_t;

typedef struct mkc_ast_check_flag_t {
  mkc_astnode_t   *vala;
  mkc_astnode_t   *stmtblock;
  int             addchk;
} mkc_ast_check_flag_t;

typedef struct mkc_ast_chk_package_t {
  mkc_astnode_t     *vala;
  mkc_astnode_t     *stmtblock;
} mkc_ast_chk_package_t;

typedef struct mkc_ast_chk_member_t {
  mkc_astnode_t     *vala;
  mkc_astnode_t     *valb;
  mkc_astnode_t     *stmtblock;
} mkc_ast_chk_member_t;

/* attributes */

typedef struct mkc_ast_attribute_t {
  mkc_astnode_t     *name;
} mkc_ast_attribute_t;

typedef struct mkc_ast_attr_alternate_t {
  mkc_astnode_t     *stmtblock;
} mkc_ast_attr_alternate_t;

typedef struct mkc_ast_attr_compflag_t {
  mkc_astnode_t     *compflaglist;
} mkc_ast_attr_compflag_t;

typedef struct mkc_ast_attr_match_t {
  mkc_astnode_t     *str;
} mkc_ast_attr_match_t;

typedef struct mkc_ast_attr_header_t {
  mkc_astnode_t     *hdrlist;
} mkc_ast_attr_header_t;

typedef struct mkc_ast_attr_linkflag_t {
  mkc_astnode_t     *linkflaglist;
} mkc_ast_attr_linkflag_t;

typedef struct mkc_ast_attr_replace_t {
  mkc_astnode_t     *str;
  mkc_astnode_t     *val;
} mkc_ast_attr_replace_t;

typedef struct mkc_astnode_t {
  union {
    mkc_ast_attribute_t         attribute;
    mkc_ast_attr_alternate_t    attr_alternate;
    mkc_ast_attr_compflag_t     attr_compflag;
    mkc_ast_attr_match_t        attr_match;
    mkc_ast_attr_header_t       attr_hdr;
    mkc_ast_attr_linkflag_t     attr_linkflag;
    mkc_ast_attr_replace_t      attr_repl;
    mkc_ast_check_t             chk_check;
    mkc_ast_check_flag_t        chk_flag;
    mkc_ast_chk_package_t       chk_package;
    mkc_ast_chk_member_t        chk_member;
    mkc_ast_stmt_chk_inc_deps_t stmt_chk_inc_deps;
    mkc_ast_conf_t              stmt_conf;
    mkc_ast_debug_t             stmt_debug;
    mkc_ast_exit_t              stmt_exit;
    mkc_ast_foreach_t           stmt_foreach;
    mkc_ast_function_t          stmt_function;
    mkc_ast_function_call_t     stmt_function_call;
    mkc_ast_if_t                stmt_if;
    mkc_ast_elseif_t            stmt_elseif;
    mkc_ast_loadcache_t         stmt_loadcache;
    mkc_ast_mark_t              stmt_mark;
    mkc_ast_print_t             stmt_print;
    mkc_ast_profile_t           stmt_profile;
    mkc_ast_project_t           stmt_project;
    mkc_ast_range_t             range;
    mkc_ast_set_t               stmt_set;
    mkc_ast_while_t             stmt_while;
    mkc_ast_list_t              list;
    mkc_ast_main_t              main;
    mkc_ast_op_t                op;
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
  mkc_astnode_t         * mainnode;
  mkc_profile_t         * profiles;
  mkc_process_t         * process;
  mkc_context_t         * context;
  mkc_astnode_t         ** nodelist;
  mkc_error_t           * mkcerr;
  mkc_log_t             * log;
  mkc_option_t          * mkcoptions;
  mkc_list_t            * funclist;
  mkc_value_t           value;
  int32_t               allocsz;
  int32_t               sz;
  int32_t               ccidx;
  int                   rdepth;
  int                   depth;
  int                   maxrdepth;
  bool                  stopprocess;
} mkc_astmain_t;

static int32_t  mkcnodenum = 0;

static int32_t mkc_ast_process (mkc_astmain_t *, mkc_astnode_t *astnode, int32_t *ifcond, int32_t *loopcond, int depth);
MKC_NODISCARD static mkc_astnode_t * mkc_astnode_init (mkc_astmain_t *astmain, int type, int32_t lineno, int colno);
static void mkc_astnode_free (void *astnode);
static mkc_value_t *mkc_ast_get_value (mkc_astmain_t *astmain, mkc_astnode_t *astnode);
static int mkc_ast_func_compare (void *a, void *b);

MKC_NODISCARD
mkc_astmain_t *
mkc_ast_init (mkc_log_t *log, mkc_option_t *mkcoptions, mkc_error_t *mkcerr)
{
  mkc_astmain_t   *astmain;

  astmain = malloc (sizeof (mkc_astmain_t));
  if (astmain == NULL) {
    return NULL;
  }
  memset (astmain, 0, sizeof (mkc_astmain_t));

  astmain->funclist = mkc_list_init (MKC_LIST_SORTED,
      NULL, mkc_ast_func_compare, mkcerr);
  astmain->mkcoptions = mkcoptions;

  astmain->profiles = mkc_profile_init (log, mkcerr, mkcoptions);
  if (astmain->profiles == NULL) {
    mkc_ast_free (astmain);
    return NULL;
  }

  astmain->context = mkc_context_init (mkcerr);
  if (astmain->context == NULL) {
    mkc_ast_free (astmain);
    return NULL;
  }

  astmain->process = mkc_process_init (astmain->profiles, log,
      astmain->context, mkcoptions, mkcerr);
  if (astmain->process == NULL) {
    mkc_ast_free (astmain);
    return NULL;
  }

  astmain->sz = 0;
  astmain->allocsz = 0;
  astmain->nodelist = NULL;

  astmain->mkcerr = mkcerr;
  astmain->log = log;
  astmain->value.sval = NULL;
  astmain->value.vtype = MKC_VT_INVALID;
  astmain->depth = 0;
  astmain->rdepth = 0;
  astmain->maxrdepth = 0;
  astmain->stopprocess = false;
  astmain->mainnode = NULL;

  return astmain;
}

int32_t
mkc_ast_start (mkc_astmain_t *astmain)
{
  int32_t         ifcond = false;
  int32_t         loopcond = true;

  astmain->rdepth = 0;
  mkc_ast_process (astmain, astmain->mainnode, &ifcond, &loopcond, 0);
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
      mkc_astnode_free (astmain->nodelist [i]);
    }
    free (astmain->nodelist);
  }
  if (astmain->process != NULL) {
    mkc_process_free (astmain->process);
  }
  if (astmain->profiles != NULL) {
    mkc_profile_free (astmain->profiles);
  }
  if (astmain->context != NULL) {
    mkc_context_free (astmain->context);
  }
  mkc_list_free (astmain->funclist);
  free (astmain);
}

/* for basic values, numbers, strings, variables */
MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_value (mkc_astmain_t *astmain, int asttype, char *str, int32_t lineno, int colno)
{
  mkc_astnode_t    *astnode = NULL;
  mkc_ast_value_t  *astvalue;
  mkc_value_t      *value;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: mk-value\n");

  astnode = mkc_astnode_init (astmain, MKC_T_VALUE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astvalue = &astnode->value;
  value = &astvalue->value;
  /* the context is unknown at this time */
  value->vctxt = MKC_VCTXT_TEMP;

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

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_value_list (mkc_astmain_t *astmain,
    mkc_astnode_t *listnode, mkc_astnode_t *vala, int32_t lineno, int colno)
{
  mkc_list_t      *tlist = NULL;
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
  mkc_ast_value_t *astvalue;
  mkc_value_t     *value;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: value-list\n");

  if (listnode == NULL) {
    mkc_astnode_t     *astnode;

    astnode = mkc_astnode_init (astmain, MKC_T_VALUE, lineno, colno);
    /* the values are already in an astnode, the values will be freed elsewhere */
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

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_stmtlist (mkc_astmain_t *astmain,
    mkc_astnode_t *stmtlist, mkc_astnode_t *stmt,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode = NULL;
  mkc_list_t      *tlist = NULL;
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

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
  mkc_list_set (tlist, &stmt, sizeof (mkc_astnode_t *), &loc);

  return stmtlist;
}

MKC_NODISCARD
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

MKC_NODISCARD
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

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_range (mkc_astmain_t *astmain,
    mkc_astnode_t *beg, mkc_astnode_t *end, mkc_astnode_t *incr,
    int32_t lineno, int colno)
{
  mkc_astnode_t     *astnode;
  mkc_ast_value_t   *astvalue;
  mkc_value_t      *value;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: range\n");

  astnode = mkc_astnode_init (astmain, MKC_T_RANGE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astvalue = &astnode->value;
  value = &astvalue->value;
  value->vctxt = MKC_VCTXT_TEMP;
  value->vtype = MKC_VT_RANGE;

  astnode->range.beg = beg;
  astnode->range.end = end;
  astnode->range.incr = incr;
  astnode->range.init = true;

  return astnode;
}

MKC_NODISCARD
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

  astnode->stmt_print.vala = vala;

  return astnode;
}

MKC_NODISCARD
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

  astnode->stmt_debug.dbga = vala;
  astnode->stmt_debug.dbgb = valb;

  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_stmt_chk_inc_deps (mkc_astmain_t *astmain,
    mkc_astnode_t *stmtblock, int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: mkcdebug\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_CHK_INC_DEPS, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->stmt_chk_inc_deps.stmtblock = stmtblock;
  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_configure (mkc_astmain_t *astmain, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: configure\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_CONFIGURE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->stmt_conf.stmtblock = stmtblock;

  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_project (mkc_astmain_t *astmain, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: project\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_PROJECT, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->stmt_project.stmtblock = stmtblock;

  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_loadcache (mkc_astmain_t *astmain,
    mkc_astnode_t *version, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: load-cache\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_LOADCACHE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->stmt_loadcache.version = version;
  astnode->stmt_loadcache.stmtblock = stmtblock;

  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_mark (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, mkc_astnode_t *valb,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: mark\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_MARK, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->stmt_mark.vala = vala;
  astnode->stmt_mark.valb = valb;

  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_set (mkc_astmain_t *astmain,
    mkc_astnode_t *nm, mkc_astnode_t *vala, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: set\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_SET, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->stmt_set.nm = nm;
  astnode->stmt_set.vala = vala;
  astnode->stmt_set.stmtblock = stmtblock;

  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_profile (mkc_astmain_t *astmain,
    mkc_astnode_t *nm, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: profile\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_PROFILE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->stmt_profile.nm = nm;
  astnode->stmt_profile.stmtblock = stmtblock;

  return astnode;
}

MKC_NODISCARD
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

  astnode->stmt_if.expr = expr;
  astnode->stmt_if.stmtblock = stmtblock;
  /* the elseif is a 'stmtlist' of elseif statements */
  astnode->stmt_if.elseif = elseif;
  astnode->stmt_if.elseblock = elseblock;

  return astnode;
}

MKC_NODISCARD
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

  astnode->stmt_elseif.expr = expr;
  astnode->stmt_elseif.stmtblock = stmtblock;

  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_else (mkc_astmain_t *astmain,
    mkc_astnode_t *ifstmt, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: else\n");

  ifstmt->stmt_if.elseblock = stmtblock;
  return ifstmt;
}

MKC_NODISCARD
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

  astnode->stmt_foreach.nm = nm;
  astnode->stmt_foreach.range = NULL;
  astnode->stmt_foreach.valuelist = list;
  astnode->stmt_foreach.stmtblock = stmtblock;

  return astnode;
}

MKC_NODISCARD
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

  astnode->stmt_foreach.nm = nm;
  astnode->stmt_foreach.range = range;
  astnode->stmt_foreach.valuelist = NULL;
  astnode->stmt_foreach.stmtblock = stmtblock;

  return astnode;
}

MKC_NODISCARD
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

  astnode->stmt_while.expr = expr;
  astnode->stmt_while.stmtblock = stmtblock;

  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_exit (mkc_astmain_t *astmain, mkc_astnode_t *vala,
    int32_t lineno, int colno)
{
  mkc_astnode_t     *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: exit\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_EXIT, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->stmt_exit.vala = vala;

  return astnode;
}

void
mkc_ast_process_include (mkc_astmain_t *astmain, mkc_astnode_t *vala,
    char *tbuff, size_t sz,
    int32_t lineno, int colno)
{
  mkc_value_t     *value = NULL;

  if (vala->asttype != MKC_T_VALUE) {
    return;
  }

  value = &vala->value.value;

  mkc_process_include (astmain->process, value, tbuff, sz);
}

MKC_NODISCARD
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

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_function (mkc_astmain_t *astmain,
    mkc_astnode_t *nm, mkc_astnode_t *argnames, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: function\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_FUNCTION, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->stmt_function.nm = nm;
  astnode->stmt_function.argnames = argnames;
  astnode->stmt_function.stmtblock = stmtblock;

  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_function_call (mkc_astmain_t *astmain,
    mkc_astnode_t *nm, mkc_astnode_t *funcargs,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: function-call\n");

  astnode = mkc_astnode_init (astmain, MKC_T_STMT_FUNCTION_CALL, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->stmt_function_call.nm = nm;
  astnode->stmt_function_call.funcargs = funcargs;

  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_check (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, mkc_astnode_t *stmtblock, mkc_astnode_token_t asttype,
    int32_t lineno, int colno)
{
  mkc_astnode_t       *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: chk-%s\n", typenames [asttype]);

  astnode = mkc_astnode_init (astmain, asttype, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->chk_check.vala = vala;
  astnode->chk_check.stmtblock = stmtblock;
  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_check_flag (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, mkc_astnode_t *stmtblock,
    int addchk, mkc_astnode_token_t asttype,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: chk-%s\n", typenames [asttype]);

  astnode = mkc_astnode_init (astmain, asttype, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->chk_flag.vala = vala;
  astnode->chk_flag.stmtblock = stmtblock;
  astnode->chk_flag.addchk = addchk;
  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_chk_package (mkc_astmain_t *astmain,
    mkc_astnode_t *vala, mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: chk-package\n");

  astnode = mkc_astnode_init (astmain, MKC_T_CHK_PACKAGE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->chk_package.vala = vala;
  astnode->chk_package.stmtblock = stmtblock;
  return astnode;
}

MKC_NODISCARD
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

  astnode->chk_member.vala = vala;
  astnode->chk_member.valb = valb;
  astnode->chk_member.stmtblock = stmtblock;
  return astnode;
}

/* attributes */

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_attr_alternate (mkc_astmain_t *astmain,
    mkc_astnode_t *stmtblock,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: attr-alternate\n");

  astnode = mkc_astnode_init (astmain, MKC_T_ATTR_ALTERNATE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->attr_alternate.stmtblock = stmtblock;
  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_attribute (mkc_astmain_t *astmain,
    mkc_astnode_t *nm, mkc_astnode_token_t asttype,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: attr-%s\n", typenames [asttype]);

  astnode = mkc_astnode_init (astmain, asttype, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->attribute.name = nm;
  return astnode;
}

MKC_NODISCARD
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

  astnode->attr_hdr.hdrlist = hdrlist;
  return astnode;
}

MKC_NODISCARD
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

  astnode->attr_compflag.compflaglist = compflaglist;
  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_attr_match (mkc_astmain_t *astmain,
    mkc_astnode_t *value,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: attr-find\n");

  astnode = mkc_astnode_init (astmain, MKC_T_ATTR_MATCH, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->attr_match.str = value;
  return astnode;
}

MKC_NODISCARD
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

  astnode->attr_linkflag.linkflaglist = linkflaglist;
  return astnode;
}

MKC_NODISCARD
mkc_astnode_t *
mkc_ast_mk_attr_replace (mkc_astmain_t *astmain,
    mkc_astnode_t *valstr, mkc_astnode_t *value,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: attr-replace\n");

  astnode = mkc_astnode_init (astmain, MKC_T_ATTR_REPLACE, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astnode->attr_repl.str = valstr;
  astnode->attr_repl.val = value;
  return astnode;
}

mkc_astnode_t *
mkc_ast_mk_main (mkc_astmain_t *astmain,
    mkc_astnode_t *stmtlist,
    int32_t lineno, int colno)
{
  mkc_astnode_t   *astnode;

  mkc_log_loc (astmain->log, MKC_LOG_AST, lineno, colno,
      "ast-mk: main\n");

  astnode = mkc_astnode_init (astmain, MKC_T_MAIN, lineno, colno);
  if (astnode == NULL) {
    return NULL;
  }

  astmain->mainnode = astnode;
  astnode->main.stmtlist = stmtlist;

  return astnode;
}

mkc_astnode_t *
mkc_ast_get_main (mkc_astmain_t *astmain)
{
  if (astmain == NULL) {
    return NULL;
  }

  return astmain->mainnode;
}

/* internal routines */

/* depth is the indentation-depth */
/* astmain->rdepth is the recursion-depth */
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

  if (mkc_error_chk_err (astmain->mkcerr) || astmain->stopprocess) {
    mkc_log (astmain->log, MKC_LOG_AST_PROCESS,
        "ast-proc: error: %d\n", mkc_error_value (astmain->mkcerr));
    /* an error occurred, stop processing */
    astmain->rdepth -= 1;
    return mkc_error_value (astmain->mkcerr);
  }

  if (astnode == NULL) {
    mkc_log (astmain->log, MKC_LOG_AST_PROCESS, "ast-proc: null\n");
    astmain->value.ival = 0;
    astmain->value.vtype = MKC_VT_INTEGER;
    astmain->rdepth -= 1;
    return astmain->value.ival;
  }

  mkc_error_set_line_col (astmain->mkcerr, astnode->lineno, astnode->colno);

  //fprintf (stderr, "%*sast-proc: %s\n", astmain->depth * 2, " ", typenames [astnode->asttype]);
  mkc_log_loc (astmain->log, MKC_LOG_AST_PROCESS, astnode->lineno, astnode->colno,
      "%*sast-proc: %s\n", astmain->depth * 2, " ", typenames [astnode->asttype]);

  switch (astnode->asttype) {
    case MKC_T_MAIN: {
      mkc_ast_process (astmain, astnode->main.stmtlist, ifcond, loopcond, depth);
      break;
    }

    case MKC_T_VALUE: {
      mkc_value_t   *value;

      value = mkc_ast_get_value (astmain, astnode);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      {
        char    *tbuff;

        tbuff = malloc (MKC_PATH_MAX);
        if (tbuff == NULL) {
          mkc_error_set (astmain->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
          break;
        }
        mkc_log_loc (astmain->log, MKC_LOG_AST,
            astnode->lineno, astnode->colno,
            "%*s%s\n", astmain->depth * 2, " ",
            mkc_value_to_str (value, tbuff, MKC_PATH_MAX));
        free (tbuff);
      }

      memcpy (&astmain->value, value, sizeof (mkc_value_t));
      break;
    }

    case MKC_T_STMTLIST: {
      mkc_listidx_t   iteridx;
      mkc_listidx_t   lidx;

      mkc_list_iter_start (astnode->stmtlist.stmtlist, &iteridx);
      while ((lidx = mkc_list_iter_next (astnode->stmtlist.stmtlist, &iteridx)) != MKC_ITER_FINISH) {
        mkc_astnode_t   **plistnode;
        mkc_astnode_t   *listnode;

        if (mkc_error_chk_err (astmain->mkcerr)) {
          break;
        }

        if (mkc_context_check (astmain->context,
              MKC_CONTEXT_LOOP | MKC_CONTEXT_CACHE)) {
          if (*loopcond == false) {
            break;
          }
        }

        plistnode = mkc_list_get_by_idx (astnode->stmtlist.stmtlist, lidx);
        listnode = *plistnode;

        if (listnode != NULL) {
          mkc_log_loc (astmain->log, MKC_LOG_AST_PROCESS, astnode->lineno, astnode->colno,
              "%*sast-proc: stmt-list: %s\n", astmain->depth * 2, " ", typenames [listnode->asttype]);

          if (listnode->asttype == MKC_T_LOOP_CONTINUE ||
              listnode->asttype == MKC_T_LOOP_BREAK) {
            if (! mkc_context_check (astmain->context, MKC_CONTEXT_LOOP)) {
              mkc_error_set (astmain->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
              break;
            }

            if (listnode->asttype == MKC_T_LOOP_BREAK) {
              *loopcond = false;
            }

            /* in both cases, the rest of the statement block */
            /* is not executed */
            break;
          }

          mkc_ast_process (astmain, listnode, ifcond, loopcond, depth);
        }
      }
      break;
    }

    /* statements */

    case MKC_T_STMT_CHK_INC_DEPS: {
      mkc_context_push (astmain->context, MKC_CONTEXT_CHK_INC_DEPS, astmain->mkcerr);
      mkc_ast_process (astmain, astnode->stmt_chk_inc_deps.stmtblock, ifcond, loopcond, depth + 1);
      mkc_context_pop (astmain->context);
      mkc_process_stmt_chk_inc_deps (astmain->process);
      break;
    }

    case MKC_T_STMT_CONFIGURE: {
      if (astnode->stmt_conf.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_CONFIGURE, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->stmt_conf.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }
      mkc_process_stmt_configure (astmain->process);
      break;
    }

    case MKC_T_STMT_DEBUG: {
      mkc_value_t   *vala;
      mkc_value_t   *valb = NULL;

      vala = mkc_ast_get_value (astmain, astnode->stmt_debug.dbga);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      /* valb can be null */
      valb = mkc_ast_get_value (astmain, astnode->stmt_debug.dbgb);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_stmt_debug (astmain->process, vala, valb);
      break;
    }

    case MKC_T_STMT_ELSEIF: {
      int32_t     rval;

      if (*ifcond) {
        /* if the starting if condition is true */
        /* no else-if statements will be processed */
        break;
      }

      mkc_ast_process (astmain, astnode->stmt_elseif.expr, ifcond, loopcond, depth);
      rval = mkc_process_condition (astmain->process, &astmain->value);
      if (rval) {
        mkc_ast_process (astmain, astnode->stmt_elseif.stmtblock, &rval, loopcond, depth + 1);
        /* if an else-if succeeds, need to pass the results back */
        /* so that other else-if and else blocks are properly processed */
        *ifcond = rval;
      }
      break;
    }

    case MKC_T_STMT_EXIT: {
      mkc_value_t   *value;
      int           exitcode;

      value = mkc_ast_get_value (astmain, astnode->stmt_exit.vala);
      exitcode = 0;
      if (value->vtype == MKC_VT_INTEGER) {
        exitcode = value->ival;
      }
      mkc_error_set (astmain->mkcerr, MKC_ERR_USER_EXIT, exitcode, NULL);
      astmain->stopprocess = true;
      break;
    }

    case MKC_T_STMT_FOREACH: {
      int32_t       rval = true;
      mkc_value_t   *valnm;
      mkc_value_t   *vallist = NULL;
      mkc_value_t   *range = NULL;
      mkc_foreach_t *pforeach;
      int32_t   count = 0;
      int32_t   limit = 10000;

      limit = mkc_process_get_loop_limit (astmain->process);
      valnm = mkc_ast_get_value (astmain, astnode->stmt_foreach.nm);
      if (astnode->stmt_foreach.valuelist != NULL) {
        vallist = mkc_ast_get_value (astmain, astnode->stmt_foreach.valuelist);
      }
      if (astnode->stmt_foreach.range != NULL) {
        mkc_astnode_t *rangenode;

        rangenode = astnode->stmt_foreach.range;
        range = &rangenode->range.value;
        mkc_ast_process (astmain, rangenode, ifcond, &rval, depth);
      }

      pforeach = mkc_process_stmt_foreach_setup (astmain->process,
          valnm, vallist, range);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      mkc_context_push (astmain->context, MKC_CONTEXT_LOOP, astmain->mkcerr);
      while (mkc_process_stmt_foreach (astmain->process, pforeach) &&
          rval &&
          count < limit) {
        if (mkc_error_chk_err (astmain->mkcerr)) {
          break;
        }

        if (! rval) {
          /* a break statement was executed */
          break;
        }

        mkc_ast_process (astmain, astnode->stmt_foreach.stmtblock, ifcond, &rval, depth + 1);
        ++count;
      }
      mkc_process_stmt_foreach_finish (astmain->process, pforeach);
      mkc_context_pop (astmain->context);
      if (count >= limit) {
        mkc_error_set (astmain->mkcerr, MKC_ERR_LOOP_LIMIT_EXCEEDED, 0, NULL);
      }
      break;
    }

    case MKC_T_STMT_FUNCTION: {
      mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
      /* no need to store the entire structure, just store the pointer */
      mkc_list_set (astmain->funclist, &astnode, sizeof (mkc_astnode_t *), &loc);
      break;
    }

    case MKC_T_STMT_FUNCTION_CALL: {
      mkc_astnode_t   tfunc;
      mkc_listidx_t   fidx;
      mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
      mkc_astnode_t   **funcp;
      mkc_astnode_t   *func;
      mkc_value_t     *valfuncargs;
      mkc_value_t     *valarglist;

      tfunc.stmt_function.nm = astnode->stmt_function_call.nm;
      tfunc.asttype = MKC_T_STMT_FUNCTION;
      func = &tfunc;

      /* the list is a list of pointers to mkc_astnode_t */
      fidx = mkc_list_find (astmain->funclist, &func, &loc);
      if (fidx == MKC_LIST_NOTFOUND) {
        mkc_error_set (astmain->mkcerr, MKC_ERR_FUNCTION_NOT_FOUND, 0, NULL);
        break;
      }

      funcp = mkc_list_get_by_idx (astmain->funclist, fidx);
      func = *funcp;

      valarglist = mkc_ast_get_value (astmain, func->stmt_function.argnames);
      valfuncargs = mkc_ast_get_value (astmain, astnode->stmt_function_call.funcargs);
      mkc_process_stmt_function_call (astmain->process, valarglist, valfuncargs);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      mkc_ast_process (astmain, func->stmt_function.stmtblock, ifcond, loopcond, depth + 1);
      mkc_profile_local_pop (astmain->profiles);

      break;
    }

    case MKC_T_STMT_IF: {
      int32_t   rval;

      mkc_ast_process (astmain, astnode->stmt_if.expr, ifcond, loopcond, depth);
      rval = mkc_process_condition (astmain->process, &astmain->value);
      if (rval) {
        mkc_ast_process (astmain, astnode->stmt_if.stmtblock, &rval, loopcond, depth + 1);
      }
      if (! rval && astnode->stmt_if.elseif != NULL) {
        mkc_ast_process (astmain, astnode->stmt_if.elseif, &rval, loopcond, depth);
      }
      if (! rval && astnode->stmt_if.elseblock != NULL) {
        mkc_ast_process (astmain, astnode->stmt_if.elseblock, &rval, loopcond, depth + 1);
      }
      break;
    }

    case MKC_T_STMT_LOADCACHE: {
      int32_t     rval = true;
      mkc_value_t *value;

      value = mkc_ast_get_value (astmain, astnode->stmt_loadcache.version);
      mkc_process_stmt_loadcache (astmain->process, value, true);
      if (astnode->stmt_loadcache.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_CACHE, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->stmt_loadcache.stmtblock, ifcond, &rval, depth + 1);
        mkc_context_pop (astmain->context);
      }
      mkc_process_stmt_loadcache (astmain->process, value, false);
      break;
    }

    case MKC_T_STMT_MARK: {
      mkc_value_t *vala;
      mkc_value_t *valb;

      vala = mkc_ast_get_value (astmain, astnode->stmt_mark.vala);
      valb = mkc_ast_get_value (astmain, astnode->stmt_mark.valb);
      mkc_process_stmt_mark (astmain->process, vala, valb);
      break;
    }

    case MKC_T_STMT_PRINT: {
      mkc_value_t   *value;

      value = mkc_ast_get_value (astmain, astnode->stmt_print.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_stmt_print (astmain->process, value, 0);
      break;
    }

    case MKC_T_STMT_PROFILE: {
      mkc_value_t   *valnm;

      valnm = mkc_ast_get_value (astmain, astnode->stmt_profile.nm);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      if (! mkc_process_profile_is_current (astmain->process, valnm)) {
        break;
      }

      mkc_process_stmt_profile (astmain->process, valnm);
      mkc_context_push (astmain->context, MKC_CONTEXT_PROFILE, astmain->mkcerr);
      mkc_ast_process (astmain, astnode->stmt_profile.stmtblock, ifcond, loopcond, depth + 1);
      mkc_context_pop (astmain->context);
      mkc_process_stmt_profile_post (astmain->process);
      break;
    }

    case MKC_T_STMT_PROJECT: {
      if (astnode->stmt_project.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_PROJECT, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->stmt_project.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }
      mkc_process_stmt_project (astmain->process);
      break;
    }

    case MKC_T_STMT_SET: {
      mkc_value_t   *valnm;
      int           rc;

      valnm = mkc_ast_get_value (astmain, astnode->stmt_set.nm);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      if (astnode->stmt_set.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_SET, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->stmt_set.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }

      mkc_ast_process (astmain, astnode->stmt_set.vala, ifcond, loopcond, depth);
      rc = mkc_process_stmt_set (astmain->process, valnm, &astmain->value);
      if (rc == MKC_OK_CHANGE) {
        *loopcond = false;
      }
      break;
    }

    case MKC_T_STMT_WHILE: {
      int32_t   rval = true;
      int32_t   count = 0;
      int32_t   limit = 10000;

      limit = mkc_process_get_loop_limit (astmain->process);
      mkc_ast_process (astmain, astnode->stmt_while.expr, ifcond, &rval, depth);
      rval = mkc_process_condition (astmain->process, &astmain->value);
      mkc_context_push (astmain->context, MKC_CONTEXT_LOOP, astmain->mkcerr);
      while (rval && count < limit) {
        mkc_ast_process (astmain, astnode->stmt_while.stmtblock, ifcond, &rval, depth + 1);
        if (! rval) {
          /* a break statement was executed */
          break;
        }
        mkc_ast_process (astmain, astnode->stmt_while.expr, ifcond, &rval, depth);
        rval = mkc_process_condition (astmain->process, &astmain->value);
        ++count;
      }
      mkc_context_pop (astmain->context);
      if (count >= limit) {
        mkc_error_set (astmain->mkcerr, MKC_ERR_LOOP_LIMIT_EXCEEDED, 0, NULL);
      }
      break;
    }

    /* attributes */

    case MKC_T_ATTR_ALTERNATE: {
      /* create a new check context */
      mkc_process_attr_alternate (astmain->process);

      mkc_context_push (astmain->context, MKC_CONTEXT_ALTERNATE, astmain->mkcerr);
      mkc_ast_process (astmain, astnode->attr_alternate.stmtblock, ifcond, loopcond, depth + 1);
      mkc_context_pop (astmain->context);
      break;
    }

    case MKC_T_ATTR_COMP_FLAGS: {
      mkc_value_t   *val;

      val = mkc_ast_get_value (astmain, astnode->attr_compflag.compflaglist);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_attr_comp_flags (astmain->process, val);
      break;
    }

    case MKC_T_ATTR_COMPILER: {
      mkc_value_t   *valnm;

      valnm = mkc_ast_get_value (astmain, astnode->attribute.name);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_attr_compiler (astmain->process, valnm);
      break;
    }

    case MKC_T_ATTR_CONTEXT:
    case MKC_T_ATTR_DEFINE_ZERO:
    case MKC_T_ATTR_LIBRARY_VERSION:
    case MKC_T_ATTR_INPUT:
    case MKC_T_ATTR_MATCH:
    case MKC_T_ATTR_METHOD:
    case MKC_T_ATTR_NAME:
    case MKC_T_ATTR_NEGATE:
    case MKC_T_ATTR_OUTPUT:
    case MKC_T_ATTR_VERSION: {
      mkc_value_t   *valnm;

      valnm = mkc_ast_get_value (astmain, astnode->attribute.name);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_attribute (astmain->process, valnm, astnode->asttype);
      break;
    }

    case MKC_T_ATTR_HEADER: {
      mkc_value_t   *val;

      val = mkc_ast_get_value (astmain, astnode->attr_hdr.hdrlist);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_attr_header (astmain->process, val);
      break;
    }

    case MKC_T_ATTR_LINK_FLAGS: {
      mkc_value_t   *val;

      val = mkc_ast_get_value (astmain, astnode->attr_linkflag.linkflaglist);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_attr_link_flags (astmain->process, val);
      break;
    }

    case MKC_T_ATTR_PATH: {
      mkc_value_t   *path;

      path = mkc_ast_get_value (astmain, astnode->attribute.name);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_attr_path (astmain->process, path);
      break;
    }

    case MKC_T_ATTR_REPLACE: {
      mkc_value_t   *str;
      mkc_value_t   *val;

      str = mkc_ast_get_value (astmain, astnode->attr_repl.str);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      val = mkc_ast_get_value (astmain, astnode->attr_repl.val);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_attr_replace (astmain->process, str, val);
      break;
    }

    /* checks */

    case MKC_T_CHK_COMP_FLAG:
    case MKC_T_CHK_LINK_FLAG: {
      mkc_value_t     *val;
      mkc_ctxt_val_t  ctxt = MKC_CONTEXT_CHECK;

      if (astnode->asttype == MKC_T_CHK_COMP_FLAG) {
        ctxt = MKC_CONTEXT_COMP_FLAG;
      }
      if (astnode->chk_flag.stmtblock != NULL) {
        mkc_context_push (astmain->context, ctxt, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->chk_flag.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }

      val = mkc_ast_get_value (astmain, astnode->chk_flag.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      astmain->value.ival = mkc_process_check_flag (astmain->process,
          val, astnode->chk_flag.addchk, astnode->asttype);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_CHK_SHELL_EXTRACT: {
      mkc_value_t   *val;

      val = mkc_ast_get_value (astmain, astnode->chk_check.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      mkc_process_chk_shell_extract (astmain->process, val);
      break;
    }

    case MKC_T_CHK_ARG_COUNT:
    case MKC_T_CHK_CONST:
    case MKC_T_CHK_DEFINE:
    case MKC_T_CHK_FUNCTION:
    case MKC_T_CHK_HEADER:
    case MKC_T_CHK_PACKAGE:
    case MKC_T_CHK_SIZE:
    case MKC_T_CHK_TYPE: {
      mkc_value_t   *val;

      if (astnode->chk_check.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_CHECK, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->chk_check.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }
      val = mkc_ast_get_value (astmain, astnode->chk_check.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }
      astmain->value.ival = mkc_process_check (astmain->process,
          val, astnode->asttype);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_CHK_STRUCT_MEMBER: {
      mkc_value_t   *vala;
      mkc_value_t   *valb;

      if (astnode->chk_member.stmtblock != NULL) {
        mkc_context_push (astmain->context, MKC_CONTEXT_CHECK, astmain->mkcerr);
        mkc_ast_process (astmain, astnode->chk_member.stmtblock, ifcond, loopcond, depth + 1);
        mkc_context_pop (astmain->context);
      }

      vala = mkc_ast_get_value (astmain, astnode->chk_member.vala);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      valb = mkc_ast_get_value (astmain, astnode->chk_member.valb);
      if (mkc_error_chk_err (astmain->mkcerr)) {
        break;
      }

      astmain->value.ival = mkc_process_chk_struct_member (astmain->process,
          vala, valb);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    /* ops */

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
    case MKC_T_OP_STR_GE:
    case MKC_T_OP_STR_EQ_REGEX:
    case MKC_T_OP_STR_NE_REGEX: {
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
        mkc_error_set (astmain->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT, 0, NULL);
        break;
      }
      astmain->value.ival = mkc_process_unary_op (astmain->process, astnode->asttype, &astmain->value);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_OP_IS_DEFINED:
    case MKC_T_OP_IS_LIST: {
      mkc_ast_process (astmain, astnode->unary_op.vala, ifcond, loopcond, depth);
      if (astmain->value.vtype == MKC_VT_INTEGER ||
          astmain->value.vtype == MKC_VT_LIST ||
          astmain->value.vtype == MKC_VT_INVALID) {
        mkc_error_set (astmain->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT, 0, NULL);
        break;
      }
      astmain->value.ival = mkc_process_unary_op (astmain->process, astnode->asttype, &astmain->value);
      astmain->value.vtype = MKC_VT_INTEGER;
      break;
    }

    case MKC_T_RANGE: {
      mkc_value_t   valbeg;
      mkc_value_t   valend;
      mkc_value_t   valincr;

      mkc_ast_process (astmain, astnode->range.beg, ifcond, loopcond, astmain->depth);
      memcpy (&valbeg, &astmain->value, sizeof (mkc_value_t));
      mkc_ast_process (astmain, astnode->range.end, ifcond, loopcond, astmain->depth);
      memcpy (&valend, &astmain->value, sizeof (mkc_value_t));
      mkc_ast_process (astmain, astnode->range.incr, ifcond, loopcond, astmain->depth);
      memcpy (&valincr, &astmain->value, sizeof (mkc_value_t));
      if (astnode->range.init) {
        mkc_process_range_init (astmain->process, &astnode->range.value,
            &valbeg, &valend, &valincr);
        astnode->range.init = false;
      }
      break;
    }

    default: {
      mkc_error_set (astmain->mkcerr, MKC_ERR_UNHANDLED_VALUE, 0, NULL);
      fprintf (stderr, "ERR: ast: unhandled value %s\n", typenames [astnode->asttype]);
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

MKC_NODISCARD
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
mkc_astnode_free (void *tastnode)
{
  mkc_astnode_t   *astnode = tastnode;

  if (astnode == NULL) {
    return;
  }

  switch (astnode->asttype) {
    case MKC_T_VALUE: {
      mkc_value_t *value;

      value = &astnode->value.value;
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
    mkc_error_set (astmain->mkcerr, MKC_ERR_PARSE_FAILURE, 0, NULL);
    return NULL;
  }
  if (astnode->asttype == MKC_T_VALUE) {
    value = &astnode->value.value;
  }

  return value;
}

static int
mkc_ast_func_compare (void *a, void *b)
{
  mkc_astnode_t   **np;
  mkc_astnode_t   *fa;
  mkc_astnode_t   *fb;
  mkc_value_t     *va;
  mkc_value_t     *vb;
  const char      *sa;
  const char      *sb;

  np = a;
  fa = *np;
  fa = fa->stmt_function.nm;
  np = b;
  fb = *np;
  fb = fb->stmt_function.nm;
  va = &fa->value.value;
  vb = &fb->value.value;
  sa = va->sval;
  sb = vb->sval;

  return strcmp (sa, sb);
}
