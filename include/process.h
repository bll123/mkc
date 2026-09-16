/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_PROCESS_H
#define INC_MKC_PROCESS_H

#include <stdint.h>

#include "asttoken.h"
#include "mkc_context.h"
#include "mkc_error.h"
#include "list.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"
#include "mkc_option.h"
#include "var.h"
#include "scopedvar.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mkc_foreach_t mkc_foreach_t;
typedef struct process_t process_t;

MKC_NODISCARD process_t *process_init (scopedvar_t *scope, mkc_log_t *log, mkc_context_t *context, mkc_option_t *mkcoptions, mkc_error_t *mkcerr);
void process_free (process_t *process);

void process_range_init (process_t *process, value_t *value, value_t *beg, value_t *end, value_t *incr);
int32_t process_condition (process_t *process, value_t *value);
int32_t process_num_op (process_t *process, astnode_token_t type, value_t *vala, value_t *valb);
int32_t process_str_op (process_t *process, astnode_token_t type, value_t *stra, value_t *strb);
int32_t process_unary_op (process_t *process, astnode_token_t type, value_t *vala);
int32_t process_other_op (process_t *process, astnode_token_t type, value_t *vala);
void process_include (process_t *process, value_t *valpath, value_t *valfn, char *tbuff, size_t sz);

mkc_foreach_t *process_stmt_foreach_setup (process_t *process, value_t *valnm, value_t *vallist);
bool process_stmt_foreach (process_t *process, mkc_foreach_t *procforeach);
void process_stmt_foreach_finish (process_t *process, mkc_foreach_t *procforeach);

int process_stmt_chk_inc_compile (process_t *process);
int process_stmt_chk_inc_deps (process_t *process);
int process_stmt_chk_inc_guards (process_t *process);
void process_stmt_build (process_t *process, value_t *vallist);
void process_stmt_configure (process_t *process);
int process_stmt_debug (process_t *process, value_t *value, value_t *subvalue);
void process_stmt_executable (process_t *process, value_t *valnm);
void process_stmt_function_call (process_t *process, value_t *valparams, value_t *valfuncargs);
void process_stmt_function_call_finish (process_t *process);
void process_stmt_loadcache (process_t *process, value_t *valvers);
void process_stmt_loadcache_post (process_t *process);
void process_stmt_mark (process_t *process, value_t *vala, value_t *valb);
void process_stmt_print (process_t *process, value_t *value, int depth);
void process_stmt_profile (process_t *process, value_t *valnm);
void process_stmt_profile_post (process_t *process);
void process_stmt_project (process_t *process, value_t *valnm);
int process_stmt_set (process_t *process, value_t *valnm, value_t *value, bool tempflag);

void process_attr_alternate (process_t *process);
void process_attribute (process_t *process, value_t *name, astnode_token_t asttype);
void process_attr_comp_flags (process_t *process, value_t *value);
void process_attr_compiler (process_t *process, value_t *name);
void process_attr_header (process_t *process, value_t *value);
void process_attr_link_flags (process_t *process, value_t *value);
void process_attr_lib_flags (process_t *process, value_t *value);
void process_attr_path (process_t *process, value_t *path);
void process_attr_replace (process_t *process, value_t *str, value_t *name);
void process_attr_source (process_t *process, value_t *value);

int32_t process_check (process_t *process, value_t *valtype, astnode_token_t asttype);
int32_t process_check_flag (process_t *process, value_t *valflag, int addchk, astnode_token_t asttype);
int32_t process_chk_struct_member (process_t *process, value_t *valstructnm, value_t *valmembernm);
int32_t process_chk_shell_extract (process_t *process, value_t *valpath);

void process_local_set (process_t *process, value_t *nmval, value_t *argval);
int32_t process_get_loop_limit (process_t *process);
void process_save_cache (process_t *process);
bool process_profile_is_current (process_t *process, value_t *valnm);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_PROCESS_H */
