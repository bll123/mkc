/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 *
 *  handles setting and getting variables within a particular scope.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "envutil.h"
#include "mkc_compiler.h"
#include "mkc_const.h"
#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_list.h"   // for the iterator enums
#include "mkc_log.h"
#include "mkc_var.h"
#include "scopedvar.h"
#include "strutil.h"
#include "value.h"

typedef struct sv_profile_t {
  mkc_varlist_t   * varlist;
  char            * name;
  int32_t         local_id;
  sv_type_t       svtype;
  mkc_compiler_t  compiler;
} sv_profile_t;

typedef struct sv_proflist_t {
  sv_profile_t    * variables;
  int             allocsz;
  int             sz;
} sv_proflist_t;

typedef struct scopedvar_t {
  /* the main hierarchy */
  sv_proflist_t       hierarchy;
  sv_proflist_t       profiles;
  mkc_option_t        * mkcoptions;
  mkc_error_t         * mkcerr;
  mkc_log_t           * log;
  /* 'current_profile' may only be 'default', or the user-selected profile */
  const char          * current_profile;
  int32_t             local_id;
  mkc_compiler_t      dfltcompiler;
  mkc_compiler_t      currcompiler;
  sv_profile_t        * active_prof;
  int                 active_idx;
  int                 standardsz;
  int                 dfltprof_idx;
  int                 currprof_idx;
  int                 comp_idx;
  bool                fromcache;
} scopedvar_t;

typedef struct sv_iter_t {
  sv_proflist_t   * profiles;
  int             idx;
  int             flags;
} sv_iter_t;

static char const * const svtypenames [] = {
  [SV_T_INTERNAL] = "internal",
  [SV_T_DFLT_PROF] = "dflt_prof",
  [SV_T_CURR_PROF] = "curr_prof",
  [SV_T_CURR_PROF_COMPILER] = "curr_prof_compiler",
  [SV_T_LOCAL] = "local",
  [SV_T_TARGET] = "target",
  [SV_T_NOT_IN_USE] = "not_in_use",
  [SV_T_SEARCH] = "search",
  [SV_T_ACTIVE] = "active",
  [SV_T_TIMESTAMP] = "timestamp",
  [SV_T_DEPENDENCY] = "dependency",
  [SV_T_PATHS] = "paths",
};

static void scopedvar_set_current_profile (scopedvar_t *scopedvar, const char *name);
static void scopedvar_set_comp_profile (scopedvar_t *scopedvar, const char *name, mkc_compiler_t compiler);
static void scopedvar_free_variables (sv_proflist_t *variables, bool hierarchyflag);
static sv_profile_t * scopedvar_create (scopedvar_t *scopedvar, sv_type_t svtype, const char *name, bool template);
static sv_profile_t * scopedvar_create_profile (scopedvar_t *scope, sv_type_t svtype, const char *name);

static void scopedvar_get_variable_str (scopedvar_t *scope, value_t *value, char *buff, size_t sz);
static void scopedvar_sub_escapes (char *buff, size_t blen);
static int32_t scopedvar_get_variable_integer (scopedvar_t *scope, value_t *value);
static value_t * scopedvar_get_variable_value (scopedvar_t *scope, const char *str);

static void scopedvar_proflist_init (sv_proflist_t *svlist);
static int scopedvar_locate_svtype (scopedvar_t *scopedvar, sv_type_t svtype);
static void scopedvar_profile_check_create (scopedvar_t *scopedvar, const char *name);
static void scopedvar_compiler_check_create (scopedvar_t *scopedvar, const char *name, mkc_compiler_t compiler);
static void scopedvar_free_vars (scopedvar_t *scopedvar);
static void scopedvar_init_vars (scopedvar_t *scopedvar, mkc_option_t *mkcoptions);
static void scopedvar_push_hierarchy (scopedvar_t *scopedvar, sv_profile_t *svprof);
static const char * scopedvar_get_active_name (scopedvar_t *scopedvar);

scopedvar_t *
scopedvar_init (mkc_log_t *log, mkc_error_t *mkcerr, mkc_option_t *mkcoptions)
{
  scopedvar_t   *scopedvar;

  scopedvar = malloc (sizeof (scopedvar_t));
  if (scopedvar == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  scopedvar->mkcoptions = mkcoptions;
  scopedvar->mkcerr = mkcerr;
  scopedvar->log = log;
  scopedvar_proflist_init (&scopedvar->profiles);
  scopedvar->local_id = 0;
  scopedvar->dfltcompiler = MKC_COMPILER_C;
  scopedvar->currcompiler = MKC_COMPILER_GENERAL;
  scopedvar->fromcache = false;
  scopedvar->current_profile = MKC_C_PROF_NAME_DEFAULT;
  scopedvar->active_prof = NULL;
  scopedvar->active_idx = -1;
  scopedvar->dfltprof_idx = -1;
  scopedvar->currprof_idx = -1;
  scopedvar->comp_idx = -1;

  scopedvar_proflist_init (&scopedvar->hierarchy);

  scopedvar_init_vars (scopedvar, mkcoptions);

  return scopedvar;
}

void
scopedvar_free (scopedvar_t *scopedvar)
{
  if (scopedvar == NULL) {
    return;
  }
  scopedvar_free_vars (scopedvar);
  scopedvar_free_variables (&scopedvar->hierarchy, true);
  free (scopedvar);
}

void
scopedvar_reset (scopedvar_t *scopedvar, mkc_option_t *mkcoptions)
{
  if (scopedvar == NULL) {
    return;
  }
  scopedvar_free_vars (scopedvar);
  scopedvar_init_vars (scopedvar, mkcoptions);
}

/* only local and target types are pushed */
void
scopedvar_push (scopedvar_t *scopedvar, sv_type_t svtype, const char *name)
{
  sv_profile_t   * svprof;

  if (svtype != SV_T_LOCAL &&
     svtype != SV_T_TARGET) {
    return;
  }

  svprof = scopedvar_create (scopedvar, svtype, name, false);
  if (svprof != NULL) {
    scopedvar_push_hierarchy (scopedvar, svprof);
  }
}

void
scopedvar_pop (scopedvar_t *scopedvar)
{
  sv_proflist_t   * proflist;
  sv_profile_t    * svprof;

  if (scopedvar == NULL) {
    return;
  }

  proflist = &scopedvar->profiles;
  if (proflist->sz <= 0) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, "scope");
    return;
  }

  /* the standard scopes should never get popped off of the stack */
  if (proflist->sz == scopedvar->standardsz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, "scope-b");
    return;
  }

  svprof = &proflist->variables [proflist->sz - 1];

  proflist->sz -= 1;
  datafree (svprof->name);
  mkc_varlist_free (svprof->varlist);
  svprof->varlist = NULL;
  svprof->svtype = SV_T_NOT_IN_USE;

  proflist = &scopedvar->hierarchy;
  svprof = &proflist->variables [proflist->sz - 1];
  svprof->svtype = SV_T_NOT_IN_USE;
  proflist->sz -= 1;
}

void
scopedvar_set_default_compiler (scopedvar_t *scopedvar, mkc_compiler_t compiler)
{
  const char  *active_name;

  if (scopedvar == NULL) {
    return;
  }

  scopedvar->dfltcompiler = compiler;
  scopedvar_compiler_check_create (scopedvar, MKC_C_PROF_NAME_DEFAULT, compiler);
  scopedvar_compiler_check_create (scopedvar, scopedvar->current_profile, compiler);
  active_name = scopedvar_get_active_name (scopedvar);
  scopedvar_set_comp_profile (scopedvar, active_name, compiler);
  /* reset the active profile */
  scopedvar_set_active_profile (scopedvar, active_name);
}

void
scopedvar_set_current_compiler (scopedvar_t *scopedvar, mkc_compiler_t compiler)
{
  const char    * active_name;

  if (scopedvar == NULL) {
    return;
  }

  scopedvar->currcompiler = compiler;
  if (compiler == MKC_COMPILER_GENERAL) {
    /* nothing to do */
    return;
  }

  scopedvar_compiler_check_create (scopedvar, MKC_C_PROF_NAME_DEFAULT, compiler);
  active_name = scopedvar_get_active_name (scopedvar);
  scopedvar_compiler_check_create (scopedvar, active_name, compiler);
  scopedvar_set_comp_profile (scopedvar, active_name, compiler);
  scopedvar->active_prof =
      &scopedvar->hierarchy.variables [scopedvar->comp_idx];
  scopedvar->active_idx = scopedvar->comp_idx;
}

void
scopedvar_set_fromcache (scopedvar_t *scopedvar, bool flag)
{
  if (scopedvar == NULL) {
    return;
  }

  scopedvar->fromcache = flag;
}

/* profile handling */

void
scopedvar_incr_local_id (scopedvar_t *scopedvar)
{
  scopedvar->local_id += 1;
}

void
scopedvar_decr_local_id (scopedvar_t *scopedvar)
{
  sv_profile_t *svprof;
  int             sz;

  sz = scopedvar->hierarchy.sz - 1;
  svprof = &scopedvar->hierarchy.variables [sz];

  if ((svprof->svtype == SV_T_LOCAL ||
      svprof->svtype == SV_T_TARGET) &&
      svprof->local_id == scopedvar->local_id) {
    scopedvar_pop (scopedvar);
  }

  scopedvar->local_id -= 1;
  if (scopedvar->local_id < 0) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_FATAL_ERROR, 0, "local-counter");
  }
}

/* the profile may be 'internal', 'default' or any other name */
/* when the cache is being loaded, or the profile is one of the */
/* namespaces, the active profile will point into the .profiles array */
void
scopedvar_set_active_profile (scopedvar_t *scopedvar, const char *name)
{
  int     idx = -1;

  /* when loading from the cache, there can be any sort of name */
  /* make sure the profile exists */
  scopedvar_profile_check_create (scopedvar, name);

  /* locate the name in the hierarchy */
  for (int i = scopedvar->hierarchy.sz - 1; i >= 0; --i) {
    sv_profile_t    *svprof;

    svprof = &scopedvar->hierarchy.variables [i];
    if (svprof->svtype != SV_T_CURR_PROF_COMPILER &&
        strcmp (svprof->name, name) == 0) {
      idx = i;
      scopedvar->active_prof = svprof;
      scopedvar->active_idx = idx;
      break;
    }
  }

  if (idx == -1) {
    /* try the profile list -- this happens when loading the cache */
    /* locate the name in the hierarchy */
    for (int i = scopedvar->profiles.sz - 1; i >= 0; --i) {
      sv_profile_t    *svprof;

      svprof = &scopedvar->profiles.variables [i];
      if (svprof->svtype != SV_T_CURR_PROF_COMPILER &&
          strcmp (svprof->name, name) == 0) {
        idx = i;
        scopedvar->active_prof = svprof;
        break;
      }
    }
  }

  if (idx == -1) {
    mkc_log (scopedvar->log, MKC_LOG_ERROR, "  scope-set-active: %s not found\n", name);
    return;
  }
}

const char *
scopedvar_get_current_profile (scopedvar_t *scopedvar)
{
  return scopedvar->current_profile;
}

void
scopedvar_reset_profile (scopedvar_t *scopedvar)
{
  scopedvar_set_active_profile (scopedvar, MKC_C_PROF_NAME_DEFAULT);
  scopedvar->currcompiler = scopedvar->dfltcompiler;
}

/* iterators */

sv_iter_t *
scopedvar_iter_start (scopedvar_t *scopedvar, sv_iter_flag_t flags)
{
  sv_iter_t   *sviter;

  sviter = malloc (sizeof (sv_iter_t));
  if (sviter == NULL) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  sviter->idx = MKC_ITER_FINISH;
  sviter->flags = flags;
  if ((flags & SV_ITER_HIERARCHY) == SV_ITER_HIERARCHY) {
    sviter->profiles = &scopedvar->hierarchy;
  }
  if ((flags & SV_ITER_PROFILES) == SV_ITER_PROFILES) {
    sviter->profiles = &scopedvar->profiles;
  }

  return sviter;
}

const char *
scopedvar_iter_next (scopedvar_t *scopedvar, sv_iter_t *sviter)
{
  if (sviter->idx == MKC_ITER_FINISH) {
    sviter->idx = 0;
  } else {
    sviter->idx += 1;
    if (sviter->idx >= sviter->profiles->sz) {
      sviter->idx = MKC_ITER_FINISH;
      return NULL;
    }
  }

  if (sviter->profiles->variables == NULL) {
    return scopedvar_iter_next (scopedvar, sviter);
  }

  return sviter->profiles->variables [sviter->idx].name;
}

void
scopedvar_iter_finish (sv_iter_t *sviter)
{
  if (sviter == NULL) {
    return;
  }

  free (sviter);
}

sv_type_t
scopedvar_iter_get_type (scopedvar_t *scopedvar, sv_iter_t *sviter)
{
  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return SV_T_NOT_IN_USE;
  }

  return sviter->profiles->variables [sviter->idx].svtype;
}

mkc_compiler_t
scopedvar_iter_get_compiler (scopedvar_t *scopedvar, sv_iter_t *sviter)
{
  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return MKC_COMPILER_GENERAL;
  }

  return sviter->profiles->variables [sviter->idx].compiler;
}

void
scopedvar_var_iter_start (scopedvar_t *scopedvar, sv_iter_t *sviter,
    mkc_varidx_t *variteridx)
{
  sv_profile_t    * svprof;
  mkc_varlist_t   * varlist = NULL;

  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return;
  }

  svprof = &sviter->profiles->variables [sviter->idx];
  varlist = svprof->varlist;
  mkc_var_iter_start (varlist, variteridx);

  return;
}

int
scopedvar_var_iter_next (scopedvar_t *scopedvar, sv_iter_t *sviter,
    mkc_varidx_t *variteridx)
{
  sv_profile_t    * svprof;
  mkc_varlist_t   * varlist = NULL;
  mkc_varidx_t    vidx;

  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return MKC_COMPILER_GENERAL;
  }

  svprof = &sviter->profiles->variables [sviter->idx];
  varlist = svprof->varlist;
  vidx = mkc_var_iter_next (varlist, variteridx);

  return vidx;
}

const char *
scopedvar_var_iter_get_name (scopedvar_t *scopedvar, sv_iter_t *sviter,
    mkc_varidx_t vidx)
{
  sv_profile_t * svprof;
  mkc_varlist_t   * varlist = NULL;
  const char      * vname;

  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return NULL;
  }

  svprof = &sviter->profiles->variables [sviter->idx];
  varlist = svprof->varlist;
  vname = mkc_var_get_name (varlist, vidx);

  return vname;
}

value_t *
scopedvar_var_iter_get_value (scopedvar_t *scopedvar, sv_iter_t *sviter,
    mkc_varidx_t vidx)
{
  sv_profile_t * svprof;
  mkc_varlist_t   * varlist = NULL;
  value_t         * value = NULL;

  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return NULL;
  }

  svprof = &sviter->profiles->variables [sviter->idx];
  varlist = svprof->varlist;
  value = mkc_var_get_value_by_idx (varlist, vidx);

  return value;
}

/* get */

int64_t
scopedvar_get_timestamp (scopedvar_t *scopedvar, sv_type_t svtype,
    const char *vname)
{
  value_t     *value;

  value = scopedvar_get_value (scopedvar, svtype, vname);
  return scopedvar_value_get_timestamp (scopedvar, value);
}

value_t *
scopedvar_get_value (scopedvar_t *scopedvar, sv_type_t svtype,
    const char *vname)
{
  sv_profile_t    * svprof;
  value_t         * value = NULL;
  mkc_varlist_t   * varlist;

  if (svtype == SV_T_ACTIVE) {
    svprof = scopedvar->active_prof;
    svtype = svprof->svtype;
  }

  /* handle the special namespaces */
  if (svtype > SV_T_NAMESPACE) {
    int     idx = -1;

    for (int i = 0; i < scopedvar->profiles.sz; ++i) {
      if (scopedvar->profiles.variables [i].svtype == svtype) {
        idx = i;
        break;
      }
    }

    if (idx == -1) {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_FATAL_ERROR, 0, NULL);
      return NULL;
    }

    svprof = &scopedvar->profiles.variables [idx];
    varlist = svprof->varlist;
    value = mkc_var_get_value (varlist, vname);

    return value;
  }

  for (int i = scopedvar->hierarchy.sz - 1; i >= 0; --i) {
    mkc_varlist_t   *varlist;

    svprof = &scopedvar->hierarchy.variables [i];
    if (svtype != SV_T_SEARCH && svprof->svtype != svtype) {
      /* if a particular scope is selected */
      continue;
    }

    varlist = svprof->varlist;
    value = mkc_var_get_value (varlist, vname);
    if (value != NULL) {
      break;
    }

    if (svtype != SV_T_SEARCH && svprof->svtype == svtype) {
      /* if a particular scope is selected */
      break;
    }
  }

  return value;
}

int32_t
scopedvar_value_get_integer (scopedvar_t *scopedvar, value_t *value)
{
  int32_t       ival = 0;

  if (value == NULL) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return 0;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
      break;
    }
    case MKC_VT_RANGE:
    case MKC_VT_TIMESTAMP: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_INTEGER: {
      ival = value->ival;
      break;
    }
    case MKC_VT_LIST: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      ival = 0;
      break;
    }
    case MKC_VT_ENV_VARIABLE: {
      char    tbuff [MKC_PATH_MAX];

      env_get (value->sval, tbuff, sizeof (tbuff));
      ival = atol (tbuff);
      break;
    }
    case MKC_VT_VARIABLE: {
      ival = scopedvar_get_variable_integer (scopedvar, value);
      break;
    }
    case MKC_VT_STRING:
    case MKC_VT_STATIC_STRING:
    case MKC_VT_QUOTED_STRING: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
  }

  mkc_log (scopedvar->log, MKC_LOG_PROCESS, "  scope-get-int: %" PRId32 "\n", ival);
  return ival;
}

int64_t
scopedvar_value_get_timestamp (scopedvar_t *scopedvar, value_t *value)
{
  int64_t    tmval = 0;

  if (value == NULL) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return 0;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_RANGE: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_TIMESTAMP: {
      tmval = value->tmval;
      break;
    }
    case MKC_VT_INTEGER:
    case MKC_VT_LIST: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      tmval = 0;
      break;
    }
    case MKC_VT_ENV_VARIABLE: {
      char    tbuff [MKC_PATH_MAX];

      env_get (value->sval, tbuff, sizeof (tbuff));
      tmval = atoll (tbuff);
      break;
    }
    case MKC_VT_VARIABLE: {
      value_t   *tvalue;

      tvalue = scopedvar_get_variable_value (scopedvar, value->sval);
      if (tvalue == NULL) {
        mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
        return 0;
      }
      if (tvalue->vtype == MKC_VT_TIMESTAMP) {
        tmval = tvalue->tmval;
      } else {
        mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      }
      break;
    }
    case MKC_VT_STRING:
    case MKC_VT_STATIC_STRING:
    case MKC_VT_QUOTED_STRING: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
  }

  mkc_log (scopedvar->log, MKC_LOG_PROCESS, "  pv-get-int: %" PRId64 "\n", tmval);
  return tmval;
}

void
scopedvar_value_get_str (scopedvar_t *scopedvar, value_t *value,
    char *buff, size_t sz)
{
  *buff = '\0';

  if (value == NULL) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
      break;
    }
    case MKC_VT_RANGE: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_INTEGER: {
      /* integers must be converted to strings, */
      /* so that substitutions can be done in a quoted string */
      snprintf (buff, sz, "%" PRId32, value->ival);
      break;
    }
    case MKC_VT_TIMESTAMP: {
      snprintf (buff, sz, "%" PRId64, value->tmval);
      break;
    }
    case MKC_VT_STRING: {
      stpecpy (buff, buff + sz, value->sval);
      break;
    }
    case MKC_VT_STATIC_STRING: {
      stpecpy (buff, buff + sz, value->sval);
      break;
    }
    case MKC_VT_QUOTED_STRING: {
      char    *tbuff;

      tbuff = scopedvar_substitute (scopedvar, value->sval, SV_SUB_ESCAPE, 0);
      stpecpy (buff, buff + sz, tbuff);
      free (tbuff);
      break;
    }
    case MKC_VT_LIST: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_ENV_VARIABLE: {
      env_get (value->sval, buff, sz);
      break;
    }
    case MKC_VT_VARIABLE: {
      scopedvar_get_variable_str (scopedvar, value, buff, sz);
      break;
    }
  }

  mkc_log (scopedvar->log, MKC_LOG_PROCESS, "  scope-get-str: %s\n", buff);
}

/* get the actual value of a value */
/* this is only an issue for env-variables, quoted strings and lists */
/* the caller is responsible for calling scopedvar_temp_value_free() */
value_t *
scopedvar_value_get_value (scopedvar_t *scopedvar, value_t *value)
{
  value_t   *tvalue;
  value_t   *nvalue;

  /* in many cases the value returned is simply the value passed in */
  nvalue = value;

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_INTEGER:
    case MKC_VT_RANGE:
    case MKC_VT_STATIC_STRING:
    case MKC_VT_STRING:
    case MKC_VT_TIMESTAMP: {
      break;
    }
    case MKC_VT_ENV_VARIABLE:
    case MKC_VT_QUOTED_STRING: {
      char    *buff;

      buff = malloc (MKC_PATH_MAX);
      if (buff == NULL) {
        mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
        return nvalue;
      }

      /* need to get the actual value */
      scopedvar_value_get_str (scopedvar, value, buff, MKC_PATH_MAX);

      tvalue = malloc (sizeof (value_t));
      if (tvalue == NULL) {
        mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
        return nvalue;
      }
      value_init (tvalue);
      tvalue->tempallocated = true;
      tvalue->vtype = MKC_VT_STRING;
      tvalue->vctxt = value->vctxt;
      tvalue->sval = buff;
      nvalue = tvalue;

      break;
    }
    case MKC_VT_VARIABLE: {
      nvalue = scopedvar_get_variable_value (scopedvar, value->sval);
      break;
    }
    case MKC_VT_LIST: {
      mkc_listidx_t     iteridx;
      mkc_listidx_t     lidx;
      mkc_list_t        *nlist;

      /* each value in a list must be processed, as the value in the list */
      /* may be an env-variable or a quoted string or a list */
      /* the list may not need substitution, but just create a new list */
      /* in all cases */

      nlist = mkc_list_init (MKC_LIST_UNSORTED, scopedvar_temp_value_free, NULL, scopedvar->mkcerr);
      mkc_list_iter_start (value->list, &iteridx);
      while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
        value_t   *lvalue;
        value_t   *tmpvalue;

        if (mkc_error_chk_err (scopedvar->mkcerr)) {
          break;
        }

        lvalue = mkc_list_get_by_idx (value->list, lidx);
        tmpvalue = scopedvar_value_get_value (scopedvar, lvalue);
        mkc_list_set (nlist, tmpvalue, sizeof (value_t));
      }

      tvalue = malloc (sizeof (value_t));
      if (tvalue == NULL) {
        mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
        return nvalue;
      }
      value_init (tvalue);
      tvalue->tempallocated = true;
      tvalue->vtype = MKC_VT_LIST;
      tvalue->vctxt = value->vctxt;
      tvalue->list = nlist;
      nvalue = tvalue;
      break;
    }
  }

  return nvalue;
}

value_t *
scopedvar_value_get_list_value (scopedvar_t *scopedvar, value_t *value)
{
  value_t    *rvalue = NULL;

  if (value == NULL) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
      break;
    }
    case MKC_VT_ENV_VARIABLE:
    case MKC_VT_INTEGER:
    case MKC_VT_QUOTED_STRING:
    case MKC_VT_STATIC_STRING:
    case MKC_VT_STRING:
    case MKC_VT_TIMESTAMP: {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_LIST: {
      rvalue = value;
      break;
    }
    case MKC_VT_RANGE: {
      rvalue = value;
      break;
    }
    case MKC_VT_VARIABLE: {
      value = scopedvar_get_variable_value (scopedvar, value->sval);
      if (value->vtype == MKC_VT_LIST) {
        rvalue = value;
      } else {
        mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      }
      break;
    }
  }

  return rvalue;
}

/* set */

void
scopedvar_set_context (scopedvar_t *scopedvar, const char *vname,
    value_ctxt_t vctxt)
{
  value_t   *value;

  if (scopedvar == NULL || vname == NULL) {
    return;
  }

  value = scopedvar_get_value (scopedvar, SV_T_SEARCH, vname);
  if (value != NULL) {
    value->vctxt = vctxt;
  }
}

int
scopedvar_set (scopedvar_t *scopedvar, sv_type_t svtype,
    const char *vname, value_t *value, value_ctxt_t vctxt)
{
  mkc_varlist_t   *varlist = NULL;
  int             rc = MKC_ERR_FAILURE;
  int             idx = -1;

  if (scopedvar == NULL) {
    return rc;
  }
  if (vname == NULL || value == NULL) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return rc;
  }

  if (svtype == SV_T_ACTIVE) {
    sv_profile_t * svprof;

    svprof = scopedvar->active_prof;
    svtype = svprof->svtype;
  }

  if (svtype > SV_T_NAMESPACE) {
    sv_profile_t * svprof = NULL;

    for (int i = 0; i < scopedvar->profiles.sz; ++i) {
      svprof = &scopedvar->profiles.variables [i];

      if (svprof->svtype == svtype) {
        idx = i;
        break;
      }
    }

    if (idx == -1) {
      return rc;
    }

    varlist = svprof->varlist;
  } else {
    sv_profile_t * svprof = NULL;

    if (svtype == SV_T_SEARCH) {
      /* search any local scopes that are on the stack */
      /* if the active_idx is reached, stop there */
      for (int i = scopedvar->hierarchy.sz - 1; i >= 0; --i) {
        svprof = &scopedvar->hierarchy.variables [i];

        if (i == scopedvar->active_idx) {
          idx = i;
          varlist = svprof->varlist;
          break;
        }

        if (svprof->svtype == SV_T_LOCAL) {
          mkc_varlist_t   *varlist;

          varlist = svprof->varlist;
          if (mkc_var_is_defined (varlist, vname)) {
            idx = i;
            break;
          }
        }
      }
    } else {
      if (svtype == SV_T_LOCAL) {
        scopedvar_push (scopedvar, SV_T_LOCAL, "local");
      }
      /* the set statement is for a specific profile */
      idx = scopedvar_locate_svtype (scopedvar, svtype);
      svprof = &scopedvar->profiles.variables [idx];
      varlist = svprof->varlist;
    }

    if (idx == -1) {
      return rc;
    }
  }

  value->vctxt = vctxt;
  mkc_var_set_fromcache (varlist, scopedvar->fromcache);
  rc = mkc_var_set (varlist, vname, value);

  return MKC_OK;
}

int
scopedvar_set_integer (scopedvar_t *scopedvar, sv_type_t svtype,
    const char *vname, int32_t ival, value_ctxt_t vctxt)
{
  int       rc = MKC_ERR_FAILURE;
  value_t   value;

  value_init (&value);
  value.ival = ival;
  value.vtype = MKC_VT_INTEGER;

  rc = scopedvar_set (scopedvar, svtype, vname, &value, vctxt);
  return rc;
}

int
scopedvar_set_timestamp (scopedvar_t *scopedvar, sv_type_t svtype,
    const char *vname, int64_t tmval, value_ctxt_t vctxt)
{
  int       rc = MKC_ERR_FAILURE;
  value_t   value;

  value_init (&value);
  value.tmval = tmval;
  value.vtype = MKC_VT_TIMESTAMP;

  rc = scopedvar_set (scopedvar, svtype, vname, &value, vctxt);
  return rc;
}

int
scopedvar_set_str (scopedvar_t *scopedvar, sv_type_t svtype,
    const char *vname, const char *str, value_ctxt_t vctxt)
{
  int       rc = MKC_ERR_FAILURE;
  value_t   value;

  value_init (&value);
  value.sval = (char *) str;
  value.vtype = MKC_VT_STRING;

  rc = scopedvar_set (scopedvar, svtype, vname, &value, vctxt);
  return rc;
}

int
scopedvar_set_list (scopedvar_t *scopedvar, sv_type_t svtype,
    const char *vname, mkc_list_t *list, value_ctxt_t vctxt)
{
  int       rc = MKC_ERR_FAILURE;
  value_t   value;

  value_init (&value);
  value.list = list;
  value.vtype = MKC_VT_LIST;

  rc = scopedvar_set (scopedvar, svtype, vname, &value, vctxt);
  return rc;
}

int
scopedvar_set_list_from_str (scopedvar_t *scopedvar,
    const char *vname, char *str, value_ctxt_t vctxt)
{
  int           rc = MKC_ERR_FAILURE;
  char          *p;
  char          *tokstr;

  p = str_token (str, " ", &tokstr);
  while (p != NULL) {
    if (mkc_error_chk_err (scopedvar->mkcerr)) {
      return MKC_ERR_FAILURE;
    }

    str_trim (p, 0);
    scopedvar_append_str_list (scopedvar, SV_T_SEARCH, vname, p, vctxt);
    p = str_token (NULL, " ", &tokstr);
  }

  return rc;
}

/* will create the value/list if it does not exist */
/* directly append the string to a value containing a list */
/* this is called using a known list value, so there are no */
/* verification checks */
int
scopedvar_append_str_list (scopedvar_t *scopedvar, sv_type_t svtype,
    const char *vname, const char *data, value_ctxt_t vctxt)
{
  value_t       *listval;
  mkc_list_t    *list;
  value_t       tvalue;


  listval = scopedvar_get_value (scopedvar, svtype, vname);
  if (listval == NULL) {
    list = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, scopedvar->mkcerr);
    scopedvar_set_list (scopedvar, svtype, vname, list, vctxt);
    listval = scopedvar_get_value (scopedvar, svtype, vname);
    mkc_list_free (list);
  }
  list = listval->list;

  if (data != NULL) {
    value_init (&tvalue);
    tvalue.vtype = MKC_VT_STRING;
    tvalue.sval = strdup (data);
    mkc_list_set (list, &tvalue, sizeof (value_t));
  }

  return MKC_OK;
}

bool
scopedvar_is_defined (scopedvar_t *scopedvar, sv_type_t svtype, const char *vname)
{
  value_t     *value;

  if (scopedvar == NULL) {
    return false;
  }

  value = scopedvar_get_value (scopedvar, svtype, vname);
  if (value == NULL) {
    return false;
  }
  return true;
}

bool
scopedvar_var_is_list (scopedvar_t *scopedvar, const char *vname)
{
  value_t     *value;
  bool        rc = false;

  if (scopedvar == NULL) {
    return rc;
  }

  value = scopedvar_get_value (scopedvar, SV_T_SEARCH, vname);
  if (value == NULL) {
    return rc;
  }

  if (value->vtype == MKC_VT_LIST || value->vtype == MKC_VT_RANGE) {
    rc = true;
  }
  return rc;
}

void
scopedvar_temp_value_free (void *tvalue)
{
  value_t   *value = tvalue;

  if (value == NULL) {
    return;
  }

  if (value->tempallocated) {
    value_free (value);
    free (value);
  }
}

/* processes the internal substitutions */
/* if the string is a variable, the final substitution is done */
/* by the caller by calling scopedvar_get_value () */
char *
scopedvar_substitute (scopedvar_t *scopedvar, const char *data,
    scopedvar_escape_t subescapeflag, int depth)
{
  size_t        len;
  char          *buff = NULL;
  char          *bp;
  const char    *srcp = data;
  const char    *endp = data;
  const char    *brpb = NULL;     // points at the start of the variable
  const char    *brpl = NULL;     // points to the closing brace
  int           count;
  size_t        pfxlen = 0;
  size_t        blen = 1;
  size_t        tlen;
  bool          isenv = false;
  char          tbuff [40];
  char          ebuff [MKC_PATH_MAX];

  if (scopedvar == NULL) {
    return NULL;
  }
  if (data == NULL) {
    return NULL;
  }

//fprintf (stderr, "%*s== data: '%s'\n", depth * 2, "", data);
  len = strlen (data);
  srcp = data;
  endp = data + len;

  brpb = strstr (data, "${");
  pfxlen = 2;
  isenv = false;
  if (brpb == NULL) {
    brpb = strstr (data, "$ENV{");
    pfxlen = 5;
    isenv = true;
  }

  if (srcp == endp) {
    buff = malloc (blen);
    *buff = '\0';
    if (subescapeflag == SV_SUB_ESCAPE) {
      scopedvar_sub_escapes (buff, blen);
    }
    return buff;
  }

  while (srcp < endp) {
    brpl = NULL;

    if (brpb != NULL) {
      const char    *tp;

      /* find the matching close-brace */
      tp = brpb;
      count = 0;
      brpl = NULL;
      while (*tp != '\0') {
        if (*tp == '{') {
          ++count;
        }
        if (*tp == '}') {
          if (count == 1) {
            brpl = tp;
            break;
          }
          --count;
        }
        ++tp;
      }

      if (brpl == NULL) {
        mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNBALANCED_BRACES, 0, NULL);
        datafree (buff);
        return NULL;
      }
    } else {
      /* ${ or $ENV{ was not found */
      /* set brpb to the last character */
      /* aaa\0 */
      /* 012 3 */
      /* tlen = 3-0 */
      brpb = endp;
    }

    /* aaa${bbb}ccc */
    /* 012345678901 */
    /* tlen = 3-0 */
    tlen = brpb - srcp;
    blen += tlen;
//fprintf (stderr, "%*schk-blen-a: %zd\n", depth * 2, "", blen);
    buff = realloc (buff, blen);
    if (tlen > 0) {
      bp = buff + blen - tlen - 1;
//fprintf (stderr, "%*schk-src-a: '%s' (%zd)\n", depth * 2, "", srcp, tlen);
      memcpy (bp, srcp, tlen);
      srcp += tlen;
    }
    buff [blen - 1] = '\0';
//fprintf (stderr, "%*sbuff-a: '%s'\n", depth * 2, "", buff);
//fprintf (stderr, "%*ssrc-a: '%s'\n", depth * 2, "", srcp);

    if (brpb != NULL && brpl != NULL) {
      char          *substr;
      char          *tstr;
      const char    *tval = NULL;

      /* aaa${bbb}ccc */
      /* 012345678901 */
      /* tlen = 8-3 = 5 - 2 = 3 */
      substr = strdup (brpb + pfxlen);
      tlen = brpl - brpb - pfxlen;
//fprintf (stderr, "%*ssubstr-len: %zd\n", depth * 2, "", tlen);
      substr [tlen] = '\0';
//fprintf (stderr, "%*ssubstr: '%s'\n", depth * 2, "", substr);
      tstr = scopedvar_substitute (scopedvar, substr, SV_NO_ESCAPE, depth + 1);
      free (substr);
//fprintf (stderr, "%*ststr: '%s'\n", depth * 2, "", tstr);

      if (isenv) {
        env_get (tstr, ebuff, sizeof (ebuff));
        tval = ebuff;
      } else {
        value_t   *value;

        value = scopedvar_get_value (scopedvar, SV_T_SEARCH, tstr);
//fprintf (stderr, "%*svalue-null %d\n", depth * 2, "", value == NULL ? 1 : 0);
        if (value != NULL && value->vtype == MKC_VT_INTEGER) {
          snprintf (tbuff, sizeof (tbuff), "%" PRId32, value->ival);
          tval = tbuff;
        }
        if (value != NULL && value->vtype == MKC_VT_STRING) {
          tval = value->sval;
        }
      }
      free (tstr);
//fprintf (stderr, "%*stval: %s\n", depth * 2, "", tbuff);

      if (tval != NULL) {
        tlen = strlen (tval);
//fprintf (stderr, "%*stval: '%s'\n", depth * 2, "", tval);
        blen += tlen;
        buff = realloc (buff, blen);
        bp = buff + blen - tlen - 1;
        memcpy (bp, tval, tlen);
//fprintf (stderr, "%*sbuff-b: '%.*s'\n", depth * 2, "", (int) blen - 1, buff);
      }
      srcp += brpl - brpb + 1;
//fprintf (stderr, "%*ssrc-b: '%s'\n", depth * 2, "", srcp);
    }
    buff [blen - 1] = '\0';

    brpb = strstr (srcp, "${");
    pfxlen = 2;
    isenv = false;
    if (brpb == NULL) {
      brpb = strstr (srcp, "$ENV{");
      pfxlen = 5;
      isenv = true;
    }
//fprintf (stderr, "%*ssrc-c: '%s'\n", depth * 2, "", srcp);
  }

//fprintf (stderr, "%*sbuff-fin: '%s'\n", depth * 2, "", buff);
  if (subescapeflag == SV_SUB_ESCAPE) {
    scopedvar_sub_escapes (buff, blen);
  }
  return buff;
}

const char *
scopedvar_type_disp (sv_type_t svtype)
{
  return svtypenames [svtype];
}

/* internal routines */

/* only called once by the initialization */
static void
scopedvar_set_current_profile (scopedvar_t *scopedvar, const char *name)
{
  sv_profile_t      *svprof = NULL;
  sv_profile_t      *fsvprof = NULL;

  for (int i = 0; i < scopedvar->profiles.sz; ++i) {
    svprof = &scopedvar->profiles.variables [i];

    if (svprof->svtype != SV_T_CURR_PROF_COMPILER &&
        strcmp (svprof->name, name) == 0) {
      fsvprof = svprof;
      break;
    }
  }

  if (fsvprof != NULL) {
    sv_profile_t    * hsvprof;

    hsvprof = &scopedvar->hierarchy.variables [scopedvar->currprof_idx];
    memcpy (hsvprof, fsvprof, sizeof (sv_profile_t));
  } else {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_FATAL_ERROR, 0, "profile not found");
    fprintf (stderr, "ERR: set-curr-profile: profile %s not found\n", name);
  }
}

static void
scopedvar_set_comp_profile (scopedvar_t *scopedvar, const char *name,
    mkc_compiler_t compiler)
{
  sv_profile_t      *svprof = NULL;
  sv_profile_t      *fsvprof = NULL;
  sv_type_t         searchtype = SV_T_CURR_PROF_COMPILER;

  if (compiler == MKC_COMPILER_GENERAL) {
    scopedvar->currcompiler = scopedvar->dfltcompiler;
  }

  for (int i = 0; i < scopedvar->profiles.sz; ++i) {
    svprof = &scopedvar->profiles.variables [i];

    if (svprof->svtype == searchtype &&
        strcmp (svprof->name, name) == 0 &&
        svprof->compiler == scopedvar->currcompiler) {
      fsvprof = svprof;
      break;
    }
  }

  if (fsvprof != NULL) {
    sv_profile_t    * hsvprof;

    hsvprof = &scopedvar->hierarchy.variables [scopedvar->comp_idx];
    memcpy (hsvprof, svprof, sizeof (sv_profile_t));
  }
}

static void
scopedvar_free_variables (sv_proflist_t *profiles, bool hierarchyflag)
{
  if (profiles != NULL) {
    if (! hierarchyflag) {
      for (int i = 0; i < profiles->sz; ++i) {
        sv_profile_t   *svprof;

        svprof = &profiles->variables [i];
        datafree (svprof->name);
        mkc_varlist_free (svprof->varlist);
      }
    }
    free (profiles->variables);
  }
}

static sv_profile_t *
scopedvar_create (scopedvar_t *scopedvar, sv_type_t svtype,
    const char *name, bool template)
{
  sv_profile_t    * svprof;

  if (svtype == SV_T_LOCAL || svtype == SV_T_TARGET) {
    char    tbuff [80];

    /* when attempting to create a local scope, first check to see */
    /* if it already exists */
    for (int i = scopedvar->profiles.sz - 1; i >= 0; --i) {
      sv_profile_t   *svprof;

      svprof = &scopedvar->profiles.variables [i];
      if (svprof->svtype == svtype) {
        if (svtype == SV_T_LOCAL &&
            svprof->local_id == scopedvar->local_id) {
          /* already exists */
          return NULL;
        }
        if (svtype == SV_T_TARGET) {
          return NULL;
        }
      }
    }

    if (svtype == SV_T_LOCAL) {
      snprintf (tbuff, sizeof (tbuff), "%s-%" PRId32, name, scopedvar->local_id);
    } else {
      stpecpy (tbuff, tbuff + sizeof (tbuff), name);
    }
    svprof = scopedvar_create_profile (scopedvar, svtype, tbuff);
  } else {
    svprof = scopedvar_create_profile (scopedvar, svtype, name);
  }

  return svprof;
}

static sv_profile_t *
scopedvar_create_profile (scopedvar_t *scopedvar,
    sv_type_t svtype, const char *name)
{
  sv_proflist_t   * profiles;
  sv_profile_t    * svprof;

  profiles = &scopedvar->profiles;
  if (profiles->sz >= profiles->allocsz) {
    profiles->allocsz += 10;
    profiles->variables = realloc (profiles->variables,
        sizeof (sv_profile_t) * profiles->allocsz);
    for (int i = profiles->sz; i < profiles->allocsz; ++i) {
      svprof = &profiles->variables [i];

      svprof->name = NULL;
      svprof->varlist = NULL;
      svprof->svtype = SV_T_NOT_IN_USE;
      svprof->compiler = MKC_COMPILER_GENERAL;
      svprof->local_id = scopedvar->local_id;
    }
  }

  svprof = &profiles->variables [profiles->sz];
  svprof->varlist = mkc_varlist_init (scopedvar->log, scopedvar->mkcerr);
  svprof->svtype = svtype;
  svprof->local_id = scopedvar->local_id;
  if (svtype == SV_T_CURR_PROF_COMPILER) {
    svprof->compiler = scopedvar->currcompiler;
  }

  if (name != NULL) {
    svprof->name = strdup (name);
  }
  profiles->sz += 1;

  return svprof;
}

static void
scopedvar_get_variable_str (scopedvar_t *scopedvar, value_t *value,
    char *buff, size_t sz)
{
  value_t     *tvalue;

  if (scopedvar == NULL) {
    return;
  }

  *buff = '\0';

  tvalue = scopedvar_get_variable_value (scopedvar, value->sval);
  if (mkc_error_chk_err (scopedvar->mkcerr)) {
    return;
  }
  if (tvalue == NULL) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
    return;
  }

  if (tvalue->vtype == MKC_VT_ENV_VARIABLE) {
    env_get (tvalue->sval, buff, sz);
    return;
  }

  if (tvalue->vtype == MKC_VT_STRING) {
    stpecpy (buff, buff + sz, tvalue->sval);
  }
  if (tvalue->vtype == MKC_VT_INTEGER) {
    snprintf (buff, sz, "%" PRId32, tvalue->ival);
  }

  {
    char    dbuff [MKC_PATH_MAX];

    mkc_log (scopedvar->log, MKC_LOG_PROCESS, "  scope-get-var-str: %s\n",
        value_to_str (tvalue, dbuff, sizeof (dbuff)));
  }
}

static void
scopedvar_sub_escapes (char *buff, size_t blen)
{
  char  *sp = buff;
  char  *dp = buff;

  while (*sp) {
    if (*sp == '\\') {
      bool  found = false;

      ++sp;
      switch (*sp) {
        case 'b': { *dp = '\b'; found = true; break; }
        case 'f': { *dp = '\f'; found = true; break; }
        case 'n': { *dp = '\n'; found = true; break; }
        case 'r': { *dp = '\r'; found = true; break; }
        case 't': { *dp = '\t'; found = true; break; }
        case 'v': { *dp = '\v'; found = true; break; }
      }
      if (found) {
        ++dp;
        continue;
      }
      /* otherwise, fall through, and the escaped character requires */
      /* no conversion */
    }

    *dp = *sp;
    ++sp;
    ++dp;
  }
  *dp = '\0';
}

static int32_t
scopedvar_get_variable_integer (scopedvar_t *scopedvar, value_t *value)
{
  int32_t     ival = 0;
  value_t     *tvalue;

  tvalue = scopedvar_get_variable_value (scopedvar, value->sval);
  if (tvalue == NULL) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
    return 0;
  }
  if (tvalue->vtype == MKC_VT_STRING) {
    ival = atol (tvalue->sval);
  }
  if (tvalue->vtype == MKC_VT_INTEGER) {
    ival = tvalue->ival;
  }

  return ival;
}

/* get-variable-value does substitutions on the variable name */
/* first. this routine should always be called before fetching the */
/* variable */
static value_t *
scopedvar_get_variable_value (scopedvar_t *scopedvar, const char *str)
{
  char        *tstr;
  value_t     *value;

  tstr = scopedvar_substitute (scopedvar, str, SV_NO_ESCAPE, 0);
  if (tstr == NULL) {
    return NULL;
  }
  value = scopedvar_get_value (scopedvar, SV_T_SEARCH, tstr);
  free (tstr);
  return value;
}

static void
scopedvar_proflist_init (sv_proflist_t *svlist)
{
  svlist->variables = NULL;
  svlist->allocsz = 0;
  svlist->sz = 0;
}

static int
scopedvar_locate_svtype (scopedvar_t *scopedvar, sv_type_t svtype)
{
  int     idx = -1;

  for (int i = scopedvar->profiles.sz - 1; i >= 0; --i) {
    if (scopedvar->profiles.variables [i].svtype == svtype) {
      idx = i;
      break;
    }
  }

  return idx;
}

static void
scopedvar_profile_check_create (scopedvar_t *scopedvar, const char *name)
{
  bool  found = false;

  /* check and see if this profile has already been created */
  for (int i = 0; i < scopedvar->profiles.sz; ++i) {
    if (strcmp (scopedvar->profiles.variables [i].name, name) == 0) {
      found = true;
      break;
    }
  }

  if (! found) {
    scopedvar_create (scopedvar, SV_T_CURR_PROF, name, false);
  }
}

static void
scopedvar_compiler_check_create (scopedvar_t *scopedvar,
    const char *name, mkc_compiler_t compiler)
{
  /* check and see if this compiler has already been created */
  for (int i = 0; i < scopedvar->profiles.sz; ++i) {
    sv_profile_t    * svprof;

    svprof = &scopedvar->profiles.variables [i];

    if (svprof->svtype == SV_T_CURR_PROF_COMPILER &&
        strcmp (svprof->name, name) == 0 &&
        svprof->compiler == compiler) {
      return;
    }
  }

  scopedvar_create (scopedvar, SV_T_CURR_PROF_COMPILER, name, false);
}

static void
scopedvar_free_vars (scopedvar_t *scopedvar)
{
  if (scopedvar == NULL) {
    return;
  }
  scopedvar_free_variables (&scopedvar->profiles, false);
}

static void
scopedvar_init_vars (scopedvar_t *scopedvar, mkc_option_t *mkcoptions)
{
  sv_profile_t    * svprof;

  /* create the standard set of scopes */
  /* when searching for a variable, the scopes will be traversed */
  /* in reverse order */
  svprof = scopedvar_create (scopedvar, SV_T_INTERNAL, MKC_C_PROF_NAME_INTERNAL, false);
  scopedvar_push_hierarchy (scopedvar, svprof);

  scopedvar->currcompiler = MKC_COMPILER_GENERAL;
  /* the default profile will hold most variables */
  /* this is useful, as the variables will be cached for */
  /* all of the different user profiles */
  svprof = scopedvar_create (scopedvar, SV_T_DFLT_PROF, MKC_C_PROF_NAME_DEFAULT, false);
  scopedvar->dfltprof_idx = scopedvar->hierarchy.sz;
  scopedvar_push_hierarchy (scopedvar, svprof);
  scopedvar->currprof_idx = scopedvar->hierarchy.sz;
  scopedvar_push_hierarchy (scopedvar, svprof);

  /* this is not a valid curr-prof-compiler, but it will be replaced */
  scopedvar->comp_idx = scopedvar->hierarchy.sz;
  scopedvar_push_hierarchy (scopedvar, svprof);

  /* the basic hierarchy is complete */
  scopedvar->standardsz = scopedvar->hierarchy.sz;

  scopedvar->current_profile = mkcoptions->currprofile;
  scopedvar->currcompiler = MKC_COMPILER_C;

  if (strcmp (scopedvar->current_profile, MKC_C_PROF_NAME_DEFAULT) != 0) {
    scopedvar_create (scopedvar, SV_T_CURR_PROF, scopedvar->current_profile, false);
  }

  /* namespaces */
  scopedvar_create (scopedvar, SV_T_TIMESTAMP, MKC_C_PROF_NAME_TIMESTAMP, false);
  scopedvar_create (scopedvar, SV_T_DEPENDENCY, MKC_C_PROF_NAME_DEPENDENCY, false);
  scopedvar_create (scopedvar, SV_T_PATHS, MKC_C_PROF_NAME_PATHS, false);

  scopedvar_set_current_profile (scopedvar, scopedvar->current_profile);
  scopedvar_set_comp_profile (scopedvar, MKC_C_PROF_NAME_DEFAULT, scopedvar->currcompiler);
  scopedvar_set_active_profile (scopedvar, MKC_C_PROF_NAME_DEFAULT);
}

static void
scopedvar_push_hierarchy (scopedvar_t *scopedvar, sv_profile_t *svprof)
{
  sv_proflist_t   * profiles;
  sv_profile_t    * hsvprof;

  profiles = &scopedvar->hierarchy;
  if (profiles->sz >= profiles->allocsz) {
    profiles->allocsz += 10;
    profiles->variables = realloc (profiles->variables,
        sizeof (sv_profile_t) * profiles->allocsz);
    for (int i = profiles->sz; i < profiles->allocsz; ++i) {
      hsvprof = &profiles->variables [i];

      hsvprof->name = NULL;
      hsvprof->varlist = NULL;
      hsvprof->svtype = SV_T_NOT_IN_USE;
      hsvprof->compiler = MKC_COMPILER_GENERAL;
      hsvprof->local_id = scopedvar->local_id;
    }
  }

  hsvprof = &profiles->variables [profiles->sz];
  memcpy (hsvprof, svprof, sizeof (sv_profile_t));
  profiles->sz += 1;
}

static const char *
scopedvar_get_active_name (scopedvar_t *scopedvar)
{
  return scopedvar->active_prof->name;
}
