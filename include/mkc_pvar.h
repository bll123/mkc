/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_PVAR_H
#define INC_MKC_PVAR_H

#include <stddef.h>
#include <stdint.h>

#include "mkc_compiler.h"
#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_log.h"
#include "mkc_profile.h"
#include "mkc_var.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mkc_pvar_t mkc_pvar_t;

mkc_pvar_t *mkc_pvar_init (mkc_profile_t *profiles, mkc_log_t *log, mkc_error_t *mkcerr);
void mkc_pvar_free (mkc_pvar_t *pvar);
int mkc_pvar_profile_set (mkc_pvar_t *pvar, const char *pname, mkc_compiler_t compiler);
int mkc_pvar_profile_set_idx (mkc_pvar_t *pvar, mkc_profidx_t pidx);
void mkc_pvar_set_fromcache (mkc_pvar_t *pvar, bool flag);

const char *mkc_pvar_name_alloc (mkc_pvar_t *pvar, const char *vname);
int mkc_pvar_set (mkc_pvar_t *pvar, const char *vname, mkc_value_t *value, mkc_var_ctxt_t vctxt);
int mkc_pvar_set_integer (mkc_pvar_t *pvar, const char *vname, int32_t ival, mkc_var_ctxt_t vctxt);
int mkc_pvar_set_str (mkc_pvar_t *pvar, const char *vname, const char *str, mkc_var_ctxt_t vctxt);
int mkc_pvar_set_list (mkc_pvar_t *pvar, const char *vname, mkc_list_t *list, mkc_var_ctxt_t vctxt);
int mkc_pvar_set_list_from_str (mkc_pvar_t *pvar, const char *vname, char *str, mkc_var_ctxt_t vctxt);
void mkc_pvar_set_context (mkc_pvar_t *pvar, const char *vname, mkc_var_ctxt_t vctxt);

mkc_varidx_t mkc_pvar_get_prof_idx (mkc_pvar_t *pvar, const char *vname);

int32_t mkc_pvar_size (mkc_pvar_t *pvar);
void mkc_pvar_iter_start (mkc_pvar_t *pvar, mkc_varidx_t *iteridx);
mkc_varidx_t mkc_pvar_iter_next (mkc_pvar_t *pvar, mkc_varidx_t *iteridx);

mkc_value_t *mkc_pvar_get_by_profile (mkc_pvar_t *pvar, const char *nm);
mkc_value_t *mkc_pvar_get_by_profidx (mkc_pvar_t *pvar, const char *nm, mkc_profidx_t pidx);
mkc_value_t *mkc_pvar_get_by_idx (mkc_pvar_t *pvar, mkc_varidx_t vidx);
const char * mkc_pvar_get_name (mkc_pvar_t *pvar, mkc_varidx_t vidx);
void mkc_pvar_get_env_str (mkc_pvar_t *pvar, const char *envstr, char *buff, size_t sz);
int32_t mkc_pvar_get_variable_integer (mkc_pvar_t *pvar, mkc_value_t *value);
void mkc_pvar_get_variable_str (mkc_pvar_t *pvar, mkc_value_t *value, char *buff, size_t sz);
mkc_value_t * mkc_pvar_get_variable_value (mkc_pvar_t *pvar, const char *str);

mkc_value_t *mkc_pvar_get_value (mkc_pvar_t *pvar, const char *vname);
//mkc_value_t *mkc_pvar_get_value (mkc_pvar_t *pvar, mkc_varidx_t vidx);

void mkc_pvar_debug (mkc_pvar_t *pvar);
int32_t mkc_pvar_value_get_integer (mkc_pvar_t *pvar, mkc_value_t *value);
void mkc_pvar_value_get_str (mkc_pvar_t *pvar, mkc_value_t *value, char *buff, size_t sz);
bool mkc_pvar_is_defined (mkc_pvar_t *pvar, const char *vname);
bool mkc_pvar_is_list (mkc_pvar_t *pvar, const char *vname);
char * mkc_pvar_substitute (mkc_pvar_t *pvar, const char *vname, int depth);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_PVAR_H */
