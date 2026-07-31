/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_SCOPEDVAR_H
#define INC_SCOPEDVAR_H

#include <stdint.h>
#include <stdbool.h>

#include "mkc_compiler.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_option.h"
#include "mkc_var.h"
#include "value.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  SV_T_INTERNAL,
  /* the default-profile holds most of the variables for all profiles */
  /* in this manner, the cache is more effective for all profiles */
  SV_T_DFLT_PROF,
  SV_T_CURR_PROF,
  /* the current-profile-compiler type encompasses all compilers */
  SV_T_CURR_PROF_COMPILER,
  SV_T_LOCAL,
  SV_T_TARGET,
  SV_T_NOT_IN_USE,
  /* for a 'get', searches the entire hierarchy */
  /* for a 'set', searches any local profiles, */
  /* then then uses the active profile */
  SV_T_SEARCH,
  /* for a 'get', only checks the active profile */
  /* for a 'set', only sets in the active profile */
  SV_T_ACTIVE,
  /* namespace is used for comparison purposes */
  /* any type following namespaces is not in the hierarchy */
  SV_T_NAMESPACE,
  SV_T_TIMESTAMP,
  SV_T_DEPENDENCY,
  SV_T_PATHS,
} sv_type_t;

typedef enum {
  SV_NO_ESCAPE,
  SV_SUB_ESCAPE,
} scopedvar_escape_t;

typedef enum {
  SV_ITER_HIERARCHY   = 0x0001,
  SV_ITER_PROFILES    = 0x0002,
} sv_iter_flag_t;

typedef struct scopedvar_t scopedvar_t;
typedef struct sv_iter_t sv_iter_t;

scopedvar_t * scopedvar_init (mkc_log_t *log, mkc_error_t *mkcerr, mkc_option_t *mkcoptions);
void scopedvar_free (scopedvar_t *scopedvar);
void scopedvar_reset (scopedvar_t *scopedvar, mkc_option_t *mkcoptions);
void scopedvar_push (scopedvar_t *scopedvar, sv_type_t svtype, const char *name);
void scopedvar_pop (scopedvar_t *scopedvar);
void scopedvar_set_default_compiler (scopedvar_t *scopedvar, mkc_compiler_t compiler);
void scopedvar_set_current_compiler (scopedvar_t *scopedvar, mkc_compiler_t compiler);
void scopedvar_reset_profile (scopedvar_t *scopedvar);
void scopedvar_set_fromcache (scopedvar_t *scopedvar, bool flag);

void scopedvar_incr_local_id (scopedvar_t *scopedvar);
void scopedvar_decr_local_id (scopedvar_t *scopedvar);
void scopedvar_set_active_profile (scopedvar_t *scopedvar, const char *name);
const char * scopedvar_get_current_profile (scopedvar_t *scopedvar);

sv_iter_t *scopedvar_iter_start (scopedvar_t *scopedvar, sv_iter_flag_t flags);
const char * scopedvar_iter_next (scopedvar_t *scopedvar, sv_iter_t *sviter);
void scopedvar_iter_finish (sv_iter_t *sviter);
sv_type_t scopedvar_iter_get_type (scopedvar_t *scopedvar, sv_iter_t *sviter);
mkc_compiler_t scopedvar_iter_get_compiler (scopedvar_t *scopedvar, sv_iter_t *sviter);
void scopedvar_var_iter_start (scopedvar_t *scopedvar, sv_iter_t *sviter, mkc_varidx_t *variteridx);
mkc_varidx_t scopedvar_var_iter_next (scopedvar_t *scopedvar, sv_iter_t *sviter, mkc_varidx_t *variteridx);
const char *scopedvar_var_iter_get_name (scopedvar_t *scopedvar, sv_iter_t *sviter, mkc_varidx_t vidx);
value_t *scopedvar_var_iter_get_value (scopedvar_t *scopedvar, sv_iter_t *sviter, mkc_varidx_t vidx);

int64_t scopedvar_get_timestamp (scopedvar_t *scopedvar, sv_type_t svtype, const char *vname);
value_t * scopedvar_get_value (scopedvar_t *scopedvar, sv_type_t svtype, const char *vname);
int32_t scopedvar_value_get_integer (scopedvar_t *scopedvar, value_t *value);
int64_t scopedvar_value_get_timestamp (scopedvar_t *scopedvar, value_t *value);
void scopedvar_value_get_str (scopedvar_t *scopedvar, value_t *value, char *buff, size_t sz);
value_t * scopedvar_value_get_value (scopedvar_t *scopedvar, value_t *value);
value_t * scopedvar_value_get_list_value (scopedvar_t *scopedvar, value_t *value);

void scopedvar_set_context (scopedvar_t *scopedvar, const char *vname, value_ctxt_t vctxt);
int scopedvar_set (scopedvar_t *scopedvar, sv_type_t svtype, const char *vname, value_t *value, value_ctxt_t vctxt);
int scopedvar_set_integer (scopedvar_t *scopedvar, sv_type_t svtype, const char *vname, int32_t ival, value_ctxt_t vctxt);
int scopedvar_set_timestamp (scopedvar_t *scopedvar, sv_type_t svtype, const char *vname, int64_t tmval, value_ctxt_t vctxt);
int scopedvar_set_str (scopedvar_t *scopedvar, sv_type_t svtype, const char *vname, const char *str, value_ctxt_t vctxt);
int scopedvar_set_list (scopedvar_t *scopedvar, sv_type_t svtype, const char *vname, mkc_list_t *list, value_ctxt_t vctxt);
int scopedvar_set_list_from_str (scopedvar_t *scopedvar, const char *vname, char *str, value_ctxt_t vctxt);
int scopedvar_append_str_list (scopedvar_t *scopedvar, sv_type_t svtype, const char *vname, const char *data, value_ctxt_t vctxt);

bool scopedvar_is_defined (scopedvar_t *scopedvar, sv_type_t svtype, const char *vname);
bool scopedvar_var_is_list (scopedvar_t *scopedvar, const char *vname);
void scopedvar_temp_value_free (void *tvalue);
char * scopedvar_substitute (scopedvar_t *scopedvar, const char *data, scopedvar_escape_t subescapeflag, int depth);

const char * scopedvar_type_disp (sv_type_t svtype);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_SCOPEDVAR_H */
