/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_PROCESS_H
#define INC_MKC_PROCESS_H

#include <stdint.h>

#include "mkc_asttoken.h"
#include "mkc_context.h"
#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"
#include "mkc_option.h"
#include "mkc_profile.h"
#include "mkc_var.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mkc_process_t mkc_process_t;

MKC_NODISCARD mkc_process_t *mkc_process_init (mkc_profile_t *profiles, mkc_log_t *log, mkc_context_t *context, mkc_option_t *mkcoptions, mkc_error_t *mkcerr);
void mkc_process_free (mkc_process_t *process);

int32_t mkc_process_condition (mkc_process_t *process, mkc_value_t *value);
int32_t mkc_process_num_op (mkc_process_t *process, int type, mkc_value_t *vala, mkc_value_t *valb);
int32_t mkc_process_str_op (mkc_process_t *process, int type, mkc_value_t *stra, mkc_value_t *strb);
int32_t mkc_process_unary_op (mkc_process_t *process, int type, mkc_value_t *vala);

void mkc_process_stmt_configure (mkc_process_t *process);
int mkc_process_stmt_debug (mkc_process_t *process, mkc_value_t *value, mkc_value_t *subvalue);
void mkc_process_stmt_loadcache (mkc_process_t *process, mkc_value_t *valvers, bool fromcache);
void mkc_process_stmt_mark (mkc_process_t *process, mkc_value_t *vala, mkc_value_t *valb);
void mkc_process_stmt_print (mkc_process_t *process, mkc_value_t *value, int depth);
void mkc_process_stmt_profile (mkc_process_t *process, mkc_value_t *valnm);
void mkc_process_stmt_profile_post (mkc_process_t *process);
void mkc_process_stmt_project (mkc_process_t *process);
int mkc_process_stmt_set (mkc_process_t *process, mkc_value_t *valnm, mkc_value_t *value);
void mkc_process_include (mkc_process_t *process, mkc_value_t *vala, char *tbuff, size_t sz);

void mkc_process_attribute (mkc_process_t *process, mkc_value_t *name, mkc_astnode_token_t asttype);
void mkc_process_attr_comp_flags (mkc_process_t *process, mkc_value_t *value);
void mkc_process_attr_header (mkc_process_t *process, mkc_value_t *value);
void mkc_process_attr_link_flags (mkc_process_t *process, mkc_value_t *value);
void mkc_process_attr_compiler (mkc_process_t *process, mkc_value_t *name);
void mkc_process_attr_replace (mkc_process_t *process, mkc_value_t *str, mkc_value_t *name);

mkc_value_t * mkc_process_get_value (mkc_process_t *process, const char *nm);

int32_t mkc_process_check (mkc_process_t *process, mkc_value_t *valtype, mkc_astnode_token_t asttype);
int32_t mkc_process_check_flag (mkc_process_t *process, mkc_value_t *valflag, int addchk, mkc_astnode_token_t asttype);
int32_t mkc_process_chk_struct_member (mkc_process_t *process, mkc_value_t *valstructnm, mkc_value_t *valmembernm);
void mkc_process_chk_shell_extract (mkc_process_t *process, mkc_value_t *valpath);

void mkc_process_local_set (mkc_process_t *process, const char *nm, const char *sval, mkc_profidx_t pidx);
bool mkc_process_profile_is_current (mkc_process_t *process, mkc_value_t *valnm);
int32_t mkc_process_get_while_limit (mkc_process_t *process);
void mkc_process_save_cache (mkc_process_t *process);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_PROCESS_H */
