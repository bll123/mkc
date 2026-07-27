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

typedef struct scopedvar_var_t {
  mkc_varlist_t     * varlist;
  char              * name;
  int32_t           local_id;
  int               local_count;
  scopedvar_type_t  svtype;
  mkc_compiler_t    compiler;
} scopedvar_var_t;

typedef struct scopedvar_varlist_t {
  scopedvar_var_t   * variables;
  int               allocsz;
  int               sz;
} scopedvar_varlist_t;

typedef struct scopedvar_t {
  /* the main hierarchy */
  scopedvar_varlist_t variables;
  /* 'profiles' will have two entries, 'default', */
  /* and the user selected profile */
  scopedvar_varlist_t profiles;
  /* 'compilervars' will have at least two entries */
  scopedvar_varlist_t compilervars;
  scopedvar_varlist_t namespaces;
  mkc_option_t        * mkcoptions;
  mkc_error_t         * mkcerr;
  mkc_log_t           * log;
  /* 'current_profile' may only be 'default', or the user-selected profile */
  const char          * current_profile;
  int32_t             local_id;
  mkc_compiler_t      dfltcompiler;
  mkc_compiler_t      currcompiler;
  int                 standardsz;
  int                 active_idx;
  int                 dfltprof_idx;
  int                 currprof_idx;
  int                 comp_idx;
  bool                fromcache;
} scopedvar_t;

typedef struct sv_iter_t {
  scopedvar_varlist_t   *variables;
  int                   idx;
  int                   flags;
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
  [SV_T_NAMESPACE] = "namespace",
  [SV_T_TIMESTAMP] = "timestamp",
  [SV_T_DEPENDENCIES] = "dependencies",
  [SV_T_PATHS] = "paths",
};

static void scopedvar_free_variables (scopedvar_varlist_t *variables, bool skip);
static void scopedvar_create (scopedvar_t *scopedvar, scopedvar_type_t svtype, const char *name, bool template);
static mkc_varlist_t * scopedvar_push_variables (scopedvar_t *scope, scopedvar_varlist_t *variables, scopedvar_type_t svtype, const char *name);
static void scopedvar_get_variable_str (scopedvar_t *scope, value_t *value, char *buff, size_t sz);
static void scopedvar_sub_escapes (char *buff, size_t blen);
static int32_t scopedvar_get_variable_integer (scopedvar_t *scope, value_t *value);
static value_t * scopedvar_get_variable_value (scopedvar_t *scope, const char *str);
static void scopedvar_svarlist_init (scopedvar_varlist_t *svlist);
static int scopedvar_locate_svtype (scopedvar_t *scopedvar, scopedvar_type_t svtype);
static void scopedvar_compiler_check_create (scopedvar_t *scopedvar, mkc_compiler_t compiler);

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
  scopedvar_svarlist_init (&scopedvar->variables);
  scopedvar_svarlist_init (&scopedvar->profiles);
  scopedvar_svarlist_init (&scopedvar->compilervars);
  scopedvar_svarlist_init (&scopedvar->namespaces);
  scopedvar->local_id = 0;
  scopedvar->dfltcompiler = MKC_COMPILER_GENERAL;
  scopedvar->currcompiler = MKC_COMPILER_GENERAL;
  scopedvar->fromcache = false;
  scopedvar->current_profile = MKC_C_PROF_DEFAULT_NAME;
  scopedvar->active_idx = -1;
  scopedvar->dfltprof_idx = -1;
  scopedvar->currprof_idx = -1;
  scopedvar->comp_idx = -1;

  /* create the standard set of scopes */
  /* when searching for a variable, the scopes will be traversed */
  /* in reverse order */
  scopedvar_create (scopedvar, SV_T_INTERNAL, MKC_C_PROF_INTERNAL_NAME, false);

  scopedvar->currcompiler = MKC_COMPILER_GENERAL;
  /* the default profile will hold most variables */
  /* this is useful, as the variables will be cached for */
  /* all of the different user profiles */
  scopedvar_create (scopedvar, SV_T_DFLT_PROF, MKC_C_PROF_DEFAULT_NAME, false);
  scopedvar->dfltprof_idx = scopedvar->variables.sz - 1;

  /* these two entries are template entries and do not own their own varlist */
  scopedvar_create (scopedvar, SV_T_CURR_PROF, MKC_C_PROF_DEFAULT_NAME, true);
  scopedvar->currprof_idx = scopedvar->variables.sz - 1;
  scopedvar_create (scopedvar, SV_T_CURR_PROF_COMPILER, MKC_C_PROF_DEFAULT_NAME, true);
  scopedvar->comp_idx = scopedvar->variables.sz - 1;

  scopedvar->currcompiler = MKC_COMPILER_C;
  if (strcmp (mkcoptions->currprofile, MKC_C_PROF_DEFAULT_NAME) != 0) {
    scopedvar_create (scopedvar, SV_T_CURR_PROF, mkcoptions->currprofile, false);
  }

  scopedvar->standardsz = scopedvar->variables.sz;

  /* namespaces */
  scopedvar_create (scopedvar, SV_T_TIMESTAMP, MKC_C_PROF_TIMESTAMP_NAME, false);
  scopedvar_create (scopedvar, SV_T_DEPENDENCIES, MKC_C_PROF_DEPENDENCIES_NAME, false);
  scopedvar_create (scopedvar, SV_T_PATHS, MKC_C_PROF_PATHS_NAME, false);

  /* default/general */
  scopedvar_set_current_profile (scopedvar, scopedvar->current_profile,
      MKC_COMPILER_GENERAL);

  return scopedvar;
}

void
scopedvar_free (scopedvar_t *scopedvar)
{
  if (scopedvar == NULL) {
    return;
  }
  scopedvar_free_variables (&scopedvar->variables, true);
  scopedvar_free_variables (&scopedvar->profiles, false);
  scopedvar_free_variables (&scopedvar->compilervars, false);
  scopedvar_free_variables (&scopedvar->namespaces, false);
  free (scopedvar);
}

/* only local and target types are pushed */
void
scopedvar_push (scopedvar_t *scopedvar, scopedvar_type_t svtype, const char *name)
{
  if (svtype != SV_T_LOCAL &&
     svtype != SV_T_TARGET) {
    return;
  }

  scopedvar_create (scopedvar, svtype, name, false);
}

void
scopedvar_pop (scopedvar_t *scopedvar)
{
  scopedvar_varlist_t   *svarlist;
  scopedvar_var_t       *scvar;

  if (scopedvar == NULL) {
    return;
  }

  svarlist = &scopedvar->variables;
  if (svarlist->sz <= 0) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, "scope");
    return;
  }

  /* the standard scopes should never get popped off of the stack */
  if (svarlist->sz == scopedvar->standardsz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, "scope-b");
    return;
  }

  scvar = &svarlist->variables [svarlist->sz - 1];
  if (scvar->local_count > 0) {
    scvar->local_count -= 1;
    return;
  }

  svarlist->sz -= 1;
  datafree (scvar->name);
  mkc_varlist_free (scvar->varlist);
  scvar->varlist = NULL;
  scvar->svtype = SV_T_NOT_IN_USE;
}

void
scopedvar_set_default_compiler (scopedvar_t *scopedvar, mkc_compiler_t compiler)
{

  if (scopedvar == NULL) {
    return;
  }

  scopedvar->dfltcompiler = compiler;
  scopedvar_compiler_check_create (scopedvar, compiler);
}

void
scopedvar_set_current_compiler (scopedvar_t *scopedvar, mkc_compiler_t compiler)
{
  if (scopedvar == NULL) {
    return;
  }

  scopedvar->currcompiler = compiler;

  scopedvar_compiler_check_create (scopedvar, compiler);
  scopedvar_set_current_profile (scopedvar, scopedvar->current_profile, compiler);
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
  scopedvar->local_id -= 1;
  if (scopedvar->local_id < 0) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_FATAL_ERROR, 0, "local-counter");
  }
}

/* in this case, the profile may be 'internal', 'default' or any other name */
void
scopedvar_set_active_profile (scopedvar_t *scopedvar, const char *name)
{
  bool    found = false;

  for (int i = 0; i < scopedvar->variables.sz; ++i) {
    if (strcmp (scopedvar->variables.variables [i].name, name) == 0) {
      scopedvar->active_idx = i;
      found = true;
      break;
    }
  }

  if (! found) {
    scopedvar_create (scopedvar, SV_T_CURR_PROF, name, false);
    if (scopedvar->currcompiler != MKC_COMPILER_GENERAL) {
      scopedvar_create (scopedvar, SV_T_CURR_PROF_COMPILER, name, false);
    }
//    scopedvar_set_active_profile (scopedvar, name);
  }
}

const char *
scopedvar_get_current_profile (scopedvar_t *scopedvar)
{
  return scopedvar->current_profile;
}

void
scopedvar_set_current_profile (scopedvar_t *scopedvar, const char *name,
    mkc_compiler_t compiler)
{
  mkc_varlist_t     *vars = NULL;
  scopedvar_var_t   *scvar = NULL;

  scopedvar->current_profile = name;

  for (int i = 0; i < scopedvar->profiles.sz; ++i) {
    scvar = &scopedvar->profiles.variables [i];

    if (strcmp (scvar->name, scopedvar->current_profile) == 0) {
      vars = scvar->varlist;
      break;
    }
  }

  if (vars == NULL) {
    if (strcmp (name, MKC_C_PROF_DEFAULT_NAME) == 0) {
      scopedvar->active_idx = scopedvar->dfltprof_idx;
    }
  }

  if (vars != NULL) {
    if (compiler == MKC_COMPILER_GENERAL) {
      scopedvar->active_idx = scopedvar->currprof_idx;
    }
    scvar = &scopedvar->variables.variables [scopedvar->currprof_idx];
    scvar->varlist = vars;
    scvar->compiler = MKC_COMPILER_GENERAL;
  }

  vars = NULL;

  if (compiler == MKC_COMPILER_GENERAL) {
    scopedvar->currcompiler = scopedvar->dfltcompiler;
  }

  for (int i = 0; i < scopedvar->compilervars.sz; ++i) {
    scvar = &scopedvar->compilervars.variables [i];

    if (strcmp (scvar->name, scopedvar->current_profile) == 0 &&
        scvar->compiler == scopedvar->currcompiler) {
      vars = scvar->varlist;
      break;
    }
  }

  if (vars != NULL) {
    if (compiler != MKC_COMPILER_GENERAL) {
      scopedvar->active_idx = scopedvar->comp_idx;
    }
    scvar = &scopedvar->variables.variables [scopedvar->comp_idx];
    scvar->varlist = vars;
    scvar->compiler = scopedvar->currcompiler;
  }
}

void
scopedvar_reset_profile (scopedvar_t *scopedvar)
{
  scopedvar_set_current_profile (scopedvar, MKC_C_PROF_DEFAULT_NAME,
      MKC_COMPILER_GENERAL);
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
    sviter->variables = &scopedvar->variables;
  }
  if ((flags & SV_ITER_USER_PROF) == SV_ITER_USER_PROF) {
    sviter->variables = &scopedvar->profiles;
  }
  if ((flags & SV_ITER_COMPILERS) == SV_ITER_COMPILERS) {
    sviter->variables = &scopedvar->compilervars;
  }
  if ((flags & SV_ITER_NAMESPACE) == SV_ITER_NAMESPACE) {
    sviter->variables = &scopedvar->namespaces;
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
    if (sviter->idx >= sviter->variables->sz) {
      sviter->idx = MKC_ITER_FINISH;
      return NULL;
    }
  }

  if (sviter->variables->variables == NULL) {
    return scopedvar_iter_next (scopedvar, sviter);
  }

  if ((sviter->flags & SV_ITER_SKIP_CURR) == SV_ITER_SKIP_CURR) {
    scopedvar_type_t    svtype;

    svtype = sviter->variables->variables [sviter->idx].svtype;
    if (svtype == SV_T_CURR_PROF ||
        svtype == SV_T_CURR_PROF_COMPILER) {
      return scopedvar_iter_next (scopedvar, sviter);
    }
  }


  return sviter->variables->variables [sviter->idx].name;
}

void
scopedvar_iter_finish (sv_iter_t *sviter)
{
  if (sviter == NULL) {
    return;
  }

  free (sviter);
}

scopedvar_type_t
scopedvar_iter_get_type (scopedvar_t *scopedvar, sv_iter_t *sviter)
{
  if (sviter->idx < 0 || sviter->idx >= sviter->variables->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return SV_T_NOT_IN_USE;
  }

  return sviter->variables->variables [sviter->idx].svtype;
}

mkc_compiler_t
scopedvar_iter_get_compiler (scopedvar_t *scopedvar, sv_iter_t *sviter)
{
  if (sviter->idx < 0 || sviter->idx >= sviter->variables->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return MKC_COMPILER_GENERAL;
  }

  return sviter->variables->variables [sviter->idx].compiler;
}

void
scopedvar_var_iter_start (scopedvar_t *scopedvar, sv_iter_t *sviter,
    mkc_varidx_t *variteridx)
{
  scopedvar_var_t * scvar;
  mkc_varlist_t   * varlist = NULL;

  if (sviter->idx < 0 || sviter->idx >= sviter->variables->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return;
  }

  scvar = &sviter->variables->variables [sviter->idx];
  varlist = scvar->varlist;
  mkc_var_iter_start (varlist, variteridx);

  return;
}

int
scopedvar_var_iter_next (scopedvar_t *scopedvar, sv_iter_t *sviter,
    mkc_varidx_t *variteridx)
{
  scopedvar_var_t * scvar;
  mkc_varlist_t   * varlist = NULL;
  mkc_varidx_t    vidx;

  if (sviter->idx < 0 || sviter->idx >= sviter->variables->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return MKC_COMPILER_GENERAL;
  }

  scvar = &sviter->variables->variables [sviter->idx];
  varlist = scvar->varlist;
  vidx = mkc_var_iter_next (varlist, variteridx);

  return vidx;
}

const char *
scopedvar_var_iter_get_name (scopedvar_t *scopedvar, sv_iter_t *sviter,
    mkc_varidx_t vidx)
{
  scopedvar_var_t * scvar;
  mkc_varlist_t   * varlist = NULL;
  const char      * vname;

  if (sviter->idx < 0 || sviter->idx >= sviter->variables->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return NULL;
  }

  scvar = &sviter->variables->variables [sviter->idx];
  varlist = scvar->varlist;
  vname = mkc_var_get_name (varlist, vidx);

  return vname;
}

value_t *
scopedvar_var_iter_get_value (scopedvar_t *scopedvar, sv_iter_t *sviter,
    mkc_varidx_t vidx)
{
  scopedvar_var_t * scvar;
  mkc_varlist_t   * varlist = NULL;
  value_t         * value = NULL;

  if (sviter->idx < 0 || sviter->idx >= sviter->variables->sz) {
    mkc_error_set (scopedvar->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return NULL;
  }

  scvar = &sviter->variables->variables [sviter->idx];
  varlist = scvar->varlist;
  value = mkc_var_get_value_by_idx (varlist, vidx);

  return value;
}

/* get */

time_t
scopedvar_get_timestamp (scopedvar_t *scopedvar, const char *vname)
{
  value_t     *value;

  value = scopedvar_get_value (scopedvar, SV_T_SEARCH, vname);
  return scopedvar_value_get_timestamp (scopedvar, value);
}

value_t *
scopedvar_get_value (scopedvar_t *scopedvar, scopedvar_type_t svtype,
    const char *vname)
{
  scopedvar_var_t * scvar;
  value_t         * value = NULL;
  mkc_varlist_t   * varlist = NULL;

  if (svtype == SV_T_ACTIVE) {
    scvar = &scopedvar->variables.variables [scopedvar->active_idx];
    svtype = scvar->svtype;
  }

  /* handle the special namespaces */
  if (svtype > SV_T_NAMESPACE) {
    int     idx = -1;

    for (int i = 0; i < scopedvar->namespaces.sz; ++i) {
      if (scopedvar->namespaces.variables [i].svtype == svtype) {
        idx = i;
        break;
      }
    }

    if (idx == -1) {
      mkc_error_set (scopedvar->mkcerr, MKC_ERR_FATAL_ERROR, 0, NULL);
      return NULL;
    }

    scvar = &scopedvar->namespaces.variables [idx];
    varlist = scvar->varlist;
    value = mkc_var_get_value (varlist, vname);

    return value;
  }

  for (int i = scopedvar->variables.sz - 1; i >= 0; --i) {
    mkc_varlist_t   *varlist;

    scvar = &scopedvar->variables.variables [i];
    if (svtype != SV_T_SEARCH && scvar->svtype != svtype) {
      /* if a particular scope is selected */
      continue;
    }

    varlist = scvar->varlist;
    value = mkc_var_get_value (varlist, vname);
    if (value != NULL) {
      break;
    }

    if (svtype != SV_T_SEARCH && scvar->svtype == svtype) {
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

time_t
scopedvar_value_get_timestamp (scopedvar_t *scopedvar, value_t *value)
{
  time_t    tmval = 0;

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
      snprintf (buff, sz, "%" PRId64, (int64_t) value->tmval);
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
        mkc_listidx_t loc = MKC_LIST_NOTFOUND;

        if (mkc_error_chk_err (scopedvar->mkcerr)) {
          break;
        }

        lvalue = mkc_list_get_by_idx (value->list, lidx);
        tmpvalue = scopedvar_value_get_value (scopedvar, lvalue);
        mkc_list_set (nlist, tmpvalue, sizeof (value_t), &loc);
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
scopedvar_set (scopedvar_t *scopedvar, scopedvar_type_t svtype,
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
    scopedvar_var_t * scvar;

    scvar = &scopedvar->variables.variables [scopedvar->active_idx];
    svtype = scvar->svtype;
  }

  if (svtype > SV_T_NAMESPACE) {
    scopedvar_var_t * scvar = NULL;

    for (int i = 0; i < scopedvar->namespaces.sz; ++i) {
      scvar = &scopedvar->namespaces.variables [i];

      if (scvar->svtype == svtype) {
        idx = i;
        break;
      }
    }

    if (idx == -1) {
      return rc;
    }

    varlist = scvar->varlist;
  } else {
    scopedvar_var_t * scvar = NULL;

    if (svtype == SV_T_SEARCH) {
      /* search any local scopes that are on the stack */
      /* if the active_idx is reached, stop there */
      for (int i = scopedvar->variables.sz - 1; i >= 0; --i) {
        scvar = &scopedvar->variables.variables [i];

        if (i == scopedvar->active_idx) {
          idx = i;
          break;
        }

        if (scvar->svtype == SV_T_LOCAL) {
          mkc_varlist_t   *varlist;

          varlist = scvar->varlist;
          if (mkc_var_is_defined (varlist, vname)) {
            idx = i;
            break;
          }
        }
      }
    } else {
      /* the set statement is for a specific profile */
      idx = scopedvar_locate_svtype (scopedvar, svtype);
      scvar = &scopedvar->variables.variables [idx];
    }

    if (idx == -1) {
      return rc;
    }

    scvar = &scopedvar->variables.variables [idx];
    varlist = scvar->varlist;
  }

  value->vctxt = vctxt;
  mkc_var_set_fromcache (varlist, scopedvar->fromcache);
  rc = mkc_var_set (varlist, vname, value);

  return MKC_OK;
}

int
scopedvar_set_integer (scopedvar_t *scopedvar, scopedvar_type_t svtype,
    const char *vname, int32_t ival, value_ctxt_t vctxt)
{
  int         rc = MKC_ERR_FAILURE;
  value_t value;

  value_init (&value);
  value.ival = ival;
  value.vtype = MKC_VT_INTEGER;

  rc = scopedvar_set (scopedvar, svtype, vname, &value, vctxt);
  return rc;
}

int
scopedvar_set_timestamp (scopedvar_t *scopedvar, scopedvar_type_t svtype,
    const char *vname, time_t tmval, value_ctxt_t vctxt)
{
  int         rc = MKC_ERR_FAILURE;
  value_t value;

  value_init (&value);
  value.tmval = tmval;
  value.vtype = MKC_VT_TIMESTAMP;

  rc = scopedvar_set (scopedvar, svtype, vname, &value, vctxt);
  return rc;
}

int
scopedvar_set_str (scopedvar_t *scopedvar, scopedvar_type_t svtype,
    const char *vname, const char *str, value_ctxt_t vctxt)
{
  int         rc = MKC_ERR_FAILURE;
  value_t value;

  value_init (&value);
  value.sval = (char *) str;
  value.vtype = MKC_VT_STRING;

  rc = scopedvar_set (scopedvar, svtype, vname, &value, vctxt);
  return rc;
}

int
scopedvar_set_list (scopedvar_t *scopedvar, scopedvar_type_t svtype,
    const char *vname, mkc_list_t *list, value_ctxt_t vctxt)
{
  int         rc = MKC_ERR_FAILURE;
  value_t value;

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
scopedvar_append_str_list (scopedvar_t *scopedvar, scopedvar_type_t svtype,
    const char *vname, const char *data, value_ctxt_t vctxt)
{
  value_t   *listval;
  mkc_list_t    *list;
  mkc_listidx_t loc;
  value_t   tvalue;


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
    mkc_list_set (list, &tvalue, sizeof (value_t), &loc);
  }

  return MKC_OK;
}

bool
scopedvar_is_defined (scopedvar_t *scopedvar, const char *vname)
{
  value_t     *value;

  if (scopedvar == NULL) {
    return false;
  }

  value = scopedvar_get_value (scopedvar, SV_T_SEARCH, vname);
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
scopedvar_type_disp (scopedvar_type_t svtype)
{
  return svtypenames [svtype];
}

/* internal routines */

static void
scopedvar_free_variables (scopedvar_varlist_t *variables, bool skip)
{
  if (variables != NULL) {
    for (int i = 0; i < variables->sz; ++i) {
      scopedvar_var_t   *scvar;

      scvar = &variables->variables [i];
      datafree (scvar->name);

      if (skip &&
          (scvar->svtype == SV_T_CURR_PROF ||
          scvar->svtype == SV_T_CURR_PROF_COMPILER)) {
        /* these two entries do not have their own varlist */
        continue;
      }

      mkc_varlist_free (scvar->varlist);
    }
    free (variables->variables);
  }
}


static void
scopedvar_create (scopedvar_t *scopedvar, scopedvar_type_t svtype,
    const char *name, bool template)
{
  if (svtype == SV_T_LOCAL || svtype == SV_T_TARGET) {
    char    tbuff [80];

    /* when attempting to create a local scope, first check to see */
    /* if it already exists */
    for (int i = scopedvar->variables.sz - 1; i >= 0; --i) {
      scopedvar_var_t   *scvar;

      scvar = &scopedvar->variables.variables [i];
      if (scvar->svtype == svtype) {
        if (svtype == SV_T_LOCAL &&
            scvar->local_id == scopedvar->local_id) {
          /* already exists, increment the count */
          scvar->local_count += 1;
          return;
        }
        if (svtype == SV_T_TARGET) {
          return;
        }
      }
    }

    if (svtype == SV_T_LOCAL) {
      snprintf (tbuff, sizeof (tbuff), "%s-%" PRId32, name, scopedvar->local_id);
    } else {
      stpecpy (tbuff, tbuff + sizeof (tbuff), name);
    }
    scopedvar_push_variables (scopedvar, &scopedvar->variables, svtype, tbuff);
  } else if (template) {
    /* this test must come before curr-prof and curr-prof-compiler */
    scopedvar_push_variables (scopedvar, &scopedvar->variables, svtype, name);
  } else if (svtype == SV_T_CURR_PROF_COMPILER) {
    scopedvar_push_variables (scopedvar, &scopedvar->compilervars, svtype, name);
  } else if (svtype == SV_T_CURR_PROF) {
    /* this only every happens twice, called from initialization */
    scopedvar_push_variables (scopedvar, &scopedvar->profiles, SV_T_CURR_PROF, name);
  } else if (svtype > SV_T_NAMESPACE) {
    /* this is only called from initialization */
    scopedvar_push_variables (scopedvar, &scopedvar->namespaces, svtype, name);
  } else {
    scopedvar_push_variables (scopedvar, &scopedvar->variables, svtype, name);
  }
}

static mkc_varlist_t *
scopedvar_push_variables (scopedvar_t *scopedvar,
    scopedvar_varlist_t *variables, scopedvar_type_t svtype, const char *name)
{
  scopedvar_var_t   *scvar;

  if (variables->sz >= variables->allocsz) {
    variables->allocsz += 10;
    variables->variables = realloc (variables->variables,
        sizeof (scopedvar_var_t) * variables->allocsz);
    for (int i = variables->sz; i < variables->allocsz; ++i) {
      scvar = &variables->variables [i];

      scvar->name = NULL;
      scvar->varlist = NULL;
      scvar->svtype = SV_T_NOT_IN_USE;
      scvar->compiler = MKC_COMPILER_GENERAL;
      scvar->local_id = scopedvar->local_id;
      scvar->local_count = 0;
    }
  }

  scvar = &variables->variables [variables->sz];
  if (&scopedvar->variables == variables &&
      (svtype == SV_T_CURR_PROF ||
      svtype == SV_T_CURR_PROF_COMPILER)) {
    /* the curr-prof and curr-prof-compiler type in the main variables */
    /* list does not own its own varlist */
    ;
  } else {
    scvar->varlist = mkc_varlist_init (scopedvar->log, scopedvar->mkcerr);
  }
  scvar->svtype = svtype;
  scvar->local_id = scopedvar->local_id;
  if (svtype == SV_T_CURR_PROF_COMPILER) {
    scvar->compiler = scopedvar->currcompiler;
  }

  if (name != NULL) {
    scvar->name = strdup (name);
  }
  variables->sz += 1;

  return scvar->varlist;
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
  value_t *value;

  tstr = scopedvar_substitute (scopedvar, str, SV_NO_ESCAPE, 0);
  value = scopedvar_get_value (scopedvar, SV_T_SEARCH, tstr);
  free (tstr);
  return value;
}

static void
scopedvar_svarlist_init (scopedvar_varlist_t *svlist)
{
  svlist->variables = NULL;
  svlist->allocsz = 0;
  svlist->sz = 0;
}

static int
scopedvar_locate_svtype (scopedvar_t *scopedvar, scopedvar_type_t svtype)
{
  int     idx = -1;

  for (int i = scopedvar->variables.sz - 1; i >= 0; --i) {
    if (scopedvar->variables.variables [i].svtype == svtype) {
      idx = i;
      break;
    }
  }

  return idx;
}

static void
scopedvar_compiler_check_create (scopedvar_t *scopedvar,
    mkc_compiler_t compiler)
{
  bool  found = false;

  /* check and see if this compiler has already been created */
  for (int i = 0; i < scopedvar->compilervars.sz; ++i) {
    if (scopedvar->compilervars.variables [i].compiler == compiler) {
      found = true;
      break;
    }
  }

  if (! found) {
    const char  * currprofile;

    currprofile = scopedvar->mkcoptions->currprofile;

    scopedvar_create (scopedvar, SV_T_CURR_PROF_COMPILER, MKC_C_PROF_DEFAULT_NAME, false);
    if (strcmp (MKC_C_PROF_DEFAULT_NAME, currprofile) != 0) {
      scopedvar_create (scopedvar, SV_T_CURR_PROF_COMPILER, currprofile, false);
    }
  }
}
