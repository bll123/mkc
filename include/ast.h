/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_AST_H
#define INC_MKC_AST_H

#include <stdint.h>
#include <stdbool.h>

#include "asttoken.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"
#include "mkc_option.h"
#include "mkc_var.h"
#include "scopedvar.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct astnode_t astnode_t;
typedef struct astmain_t astmain_t;

MKC_NODISCARD astnode_t * ast_mk_value (astmain_t *astmain, astnode_token_t asttype, char *str, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_value_list (astmain_t *astmain, astnode_t *list, astnode_t *vala, int32_t lineno, int colno);

MKC_NODISCARD astnode_t * ast_mk_op (astmain_t *astmain, astnode_t *vala, astnode_token_t op, astnode_t *valb, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_value_range (astmain_t *astmain, astnode_t *beg, astnode_t *end, astnode_t *incr, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_unary_op (astmain_t *astmain, astnode_t *value, int op, int32_t lineno, int colno);

MKC_NODISCARD astnode_t * ast_mk_elseif (astmain_t *astmain, astnode_t *expr, astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_foreach (astmain_t *astmain, astnode_t *nm, astnode_t *list, astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_foreach_range (astmain_t *astmain, astnode_t *nm, astnode_t *range, astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_function (astmain_t *astmain, astnode_t *nm, astnode_t *argnames, astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_function_call (astmain_t *astmain, astnode_t *nm, astnode_t *funcargs, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_func_return (astmain_t *astmain, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_if (astmain_t *astmain, astnode_t *expr, astnode_t *stmtblock, astnode_t *elseif, astnode_t *elseblock, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_loop_control (astmain_t *astmain, astnode_token_t type, int32_t lineno, int colno);
astnode_t * ast_mk_main (astmain_t *astmain, astnode_t *stmtlist, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_stmtlist (astmain_t *astmain, astnode_t *list, astnode_t *vala, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_while (astmain_t *astmain, astnode_t *expr, astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_exit (astmain_t *astmain, astnode_t *vala, int32_t lineno, int colno);
void ast_process_include (astmain_t *astmain, astnode_t *path, astnode_t *fn, char *tbuff, size_t sz, int32_t lineno, int colno);

/* generics */
MKC_NODISCARD astnode_t * ast_mk_stmt_stmtblock (astmain_t *astmain, astnode_t *stmtblock, astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_stmt_val_stmtblock (astmain_t *astmain, astnode_t *val, astnode_t *stmtblock, astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_stmt_val_val (astmain_t *astmain, astnode_t *vala, astnode_t *valb, astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_stmt_val (astmain_t *astmain, astnode_t *vala, astnode_token_t asttype, int32_t lineno, int colno);

MKC_NODISCARD astnode_t * ast_mk_stmt_executable (astmain_t *astmain, astnode_t *name, astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_loadcache (astmain_t *astmain, astnode_t *vers, astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_set (astmain_t *astmain, astnode_t *nm, astnode_t *expr, astnode_t *stmtblock, bool local, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_set_value (astmain_t *astmain, astnode_t *nm, astnode_t *vala, int32_t lineno, int colno);

/* checks */
MKC_NODISCARD astnode_t * ast_mk_check (astmain_t *astmain, astnode_t *vala, astnode_t *stmtblock, astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_check_flag (astmain_t *astmain, astnode_t *vala, astnode_t *stmtblock, int addflag, astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_chk_package (astmain_t *astmain, astnode_t *vala, astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_chk_struct_member (astmain_t *astmain, astnode_t *vala, astnode_t *valb, astnode_t *stmtblock, int32_t lineno, int colno);

/* attribute statements */
MKC_NODISCARD astnode_t * ast_mk_attribute (astmain_t *astmain, astnode_t *name, astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_attr_nodelist (astmain_t *astmain, astnode_t *list, astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_attr_stmtblock (astmain_t *astmain, astnode_t *stmtblock, astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_attr_match (astmain_t *astmain, astnode_t *value, int32_t lineno, int colno);
MKC_NODISCARD astnode_t * ast_mk_attr_replace (astmain_t *astmain, astnode_t *valstr, astnode_t *value, int32_t lineno, int colno);

MKC_NODISCARD astmain_t * ast_init (mkc_log_t *log, mkc_option_t *mkcoptions, mkc_error_t *mkcerr);
int32_t ast_start (astmain_t *);
void ast_free (astmain_t *astmain);
astnode_t * ast_get_main (astmain_t *astmain);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_AST_H */
