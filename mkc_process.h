/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#pragma once

#include <stdint.h>

#include "mkc_context.h"
#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_log.h"
#include "mkc_profile.h"
#include "mkc_var.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mkc_process_t mkc_process_t;

mkc_process_t *mkc_process_init (mkc_profile_t *profiles, mkc_log_t *log, mkc_context_t *context, mkc_error_t *mkcerr);
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

void mkc_process_attr_comp_flags (mkc_process_t *process, mkc_value_t *value);
void mkc_process_attr_compiler (mkc_process_t *process, mkc_value_t *name);
void mkc_process_attr_context (mkc_process_t *process, mkc_value_t *context);
void mkc_process_attr_define_zero (mkc_process_t *process);
void mkc_process_attr_header (mkc_process_t *process, mkc_value_t *value);
void mkc_process_attr_input (mkc_process_t *process, mkc_value_t *name);
void mkc_process_attr_link_flags (mkc_process_t *process, mkc_value_t *value);
void mkc_process_attr_method (mkc_process_t *process, mkc_value_t *method);
void mkc_process_attr_name (mkc_process_t *process, mkc_value_t *valnm);
void mkc_process_attr_negate (mkc_process_t *process);
void mkc_process_attr_output (mkc_process_t *process, mkc_value_t *name);

mkc_value_t * mkc_process_get_value (mkc_process_t *process, const char *nm);

int32_t mkc_process_chk_compiler_flag (mkc_process_t *process, mkc_value_t *valflag, int addchk);
int32_t mkc_process_chk_define (mkc_process_t *process, mkc_value_t *valtype);
int32_t mkc_process_chk_function (mkc_process_t *process, mkc_value_t *valfuncnm);
int32_t mkc_process_chk_link_flag (mkc_process_t *process, mkc_value_t *valflag, int addchk);
int32_t mkc_process_chk_size (mkc_process_t *process, mkc_value_t *valtype);
int32_t mkc_process_chk_struct_member (mkc_process_t *process, mkc_value_t *valstructnm, mkc_value_t *valmembernm);
int32_t mkc_process_chk_type (mkc_process_t *process, mkc_value_t *valtype);

void mkc_process_local_set (mkc_process_t *process, const char *nm, const char *sval, mkc_profidx_t pidx);
bool mkc_process_profile_is_current (mkc_process_t *process, mkc_value_t *valnm);
int32_t mkc_process_get_while_limit (mkc_process_t *process);
void mkc_process_save_cache (mkc_process_t *process);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif
