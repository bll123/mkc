/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_AST_H
#define INC_MKC_AST_H

#include <stdint.h>
#include <stdbool.h>

#include "mkc_asttoken.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_option.h"
#include "mkc_var.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

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
mkc_astnode_t * mkc_ast_mk_main (mkc_astmain_t *astmain, mkc_astnode_t *stmtlist, int32_t lineno, int colno);
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
mkc_astnode_t * mkc_ast_mk_check (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, mkc_astnode_token_t asttype, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_check_flag (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int addflag, mkc_astnode_token_t asttype, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_chk_package (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *stmtblock, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_chk_struct_member (mkc_astmain_t *astmain, mkc_astnode_t *vala, mkc_astnode_t *valb, mkc_astnode_t *stmtblock, int32_t lineno, int colno);

/* attribute statements */
mkc_astnode_t * mkc_ast_mk_attribute (mkc_astmain_t *astmain, mkc_astnode_t *name, mkc_astnode_token_t asttype, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_compflags (mkc_astmain_t *astmain, mkc_astnode_t *compflaglist, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_header (mkc_astmain_t *astmain, mkc_astnode_t *hdrlist, int32_t lineno, int colno);
mkc_astnode_t * mkc_ast_mk_attr_linkflags (mkc_astmain_t *astmain, mkc_astnode_t *linkflaglist, int32_t lineno, int colno);

mkc_astmain_t * mkc_ast_init (mkc_log_t *log, mkc_option_t *mkcoptions, mkc_error_t *mkcerr);
int32_t mkc_ast_start (mkc_astmain_t *);
void mkc_ast_free (mkc_astmain_t *astmain);
mkc_astnode_t * mkc_ast_get_main (mkc_astmain_t *astmain);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_AST_H */
