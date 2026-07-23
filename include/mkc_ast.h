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
#include "scope.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mkc_astnode_t mkc_astnode_t;
typedef struct mkc_astmain_t mkc_astmain_t;

MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_value (mkc_astmain_t *astmain, mkc_astnode_token_t asttype, char *str, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_value_list (mkc_astmain_t *astmain, mkc_astnode_t *list, mkc_astnode_t *vala, int32_t lineno, int colno);

MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_op (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_token_t op, mkc_astnode_t *valb, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_value_range (mkc_astmain_t *astmain, mkc_astnode_t *beg, mkc_astnode_t *end, mkc_astnode_t *incr, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_unary_op (mkc_astmain_t *astmain, mkc_astnode_t *value, int op, int32_t lineno, int colno);

MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_elseif (mkc_astmain_t *astmain, mkc_astnode_t *expr, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_foreach (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *list, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_foreach_range (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *range, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_function (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *argnames, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_function_call (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *funcargs, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_func_return (mkc_astmain_t *astmain, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_if (mkc_astmain_t *astmain, mkc_astnode_t *expr, mkc_astnode_t *stmtblock, mkc_astnode_t *elseif, mkc_astnode_t *elseblock, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_loop_control (mkc_astmain_t *astmain, mkc_astnode_token_t type, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_main (mkc_astmain_t *astmain, mkc_astnode_t *stmtlist, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_stmtlist (mkc_astmain_t *astmain, mkc_astnode_t *list, mkc_astnode_t *vala, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_while (mkc_astmain_t *astmain, mkc_astnode_t *expr, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_exit (mkc_astmain_t *astmain, mkc_astnode_t *vala, int32_t lineno, int colno);
void mkc_ast_process_include (mkc_astmain_t *astmain, mkc_astnode_t *path, mkc_astnode_t *fn, char *tbuff, size_t sz, int32_t lineno, int colno);

MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_stmt_stmtblock (mkc_astmain_t *astmain, mkc_astnode_t *stmtblock, mkc_astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_debug (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *valb, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_stmt_executable (mkc_astmain_t *astmain, mkc_astnode_t *name, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_loadcache (mkc_astmain_t *astmain, mkc_astnode_t *vers, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_mark (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *valb, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_print (mkc_astmain_t *astmain, mkc_astnode_t *vala, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_profile (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_set (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *expr, mkc_astnode_t *stmtblock, bool tempflag, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_set_value (mkc_astmain_t *astmain, mkc_astnode_t *nm, mkc_astnode_t *vala, int32_t lineno, int colno);

/* checks */
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_check (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, mkc_astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_check_flag (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int addflag, mkc_astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_chk_package (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_chk_struct_member (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *valb, mkc_astnode_t *stmtblock, int32_t lineno, int colno);

/* attribute statements */
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_attr_alternate (mkc_astmain_t *astmain, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_attribute (mkc_astmain_t *astmain, mkc_astnode_t *name, mkc_astnode_token_t asttype, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_attr_compflags (mkc_astmain_t *astmain, mkc_astnode_t *compflaglist, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_attr_match (mkc_astmain_t *astmain, mkc_astnode_t *value, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_attr_header (mkc_astmain_t *astmain, mkc_astnode_t *hdrlist, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_attr_linkflags (mkc_astmain_t *astmain, mkc_astnode_t *linkflaglist, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_attr_replace (mkc_astmain_t *astmain, mkc_astnode_t *valstr, mkc_astnode_t *value, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_attr_source (mkc_astmain_t *astmain, mkc_astnode_t *srclist, int32_t lineno, int colno);
MKC_NODISCARD mkc_astnode_t * mkc_ast_mk_attr_success_fail (mkc_astmain_t *astmain, mkc_astnode_t *stmtblock, mkc_astnode_token_t asttype, int32_t lineno, int colno);

MKC_NODISCARD mkc_astmain_t * mkc_ast_init (mkc_log_t *log, mkc_option_t *mkcoptions, mkc_error_t *mkcerr);
int32_t mkc_ast_start (mkc_astmain_t *);
void mkc_ast_free (mkc_astmain_t *astmain);
mkc_astnode_t * mkc_ast_get_main (mkc_astmain_t *astmain);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_AST_H */
