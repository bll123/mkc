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

#include "dict.h"
#include "dictdict.h"
#include "envutil.h"
#include "mkc_compiler.h"
#include "const.h"
#include "mkc_def.h"
#include "mkc_error.h"
#include "list.h"   // for the iterator enums
#include "mkc_log.h"
#include "var.h"
#include "scopedvar.h"
#include "strutil.h"
#include "value.h"

typedef struct sv_profile_t {
  varlist_t       * varlist;
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
  [SV_T_ACTIVE] = "active",
  [SV_T_BUILD_DATA] = "builddata",
  [SV_T_CURR_PROF_COMPILER] = "curr_prof_compiler",
  [SV_T_CURR_PROF] = "curr_prof",
  [SV_T_DFLT_PROF] = "dflt_prof",
  [SV_T_INTERNAL] = "internal",
  [SV_T_LOCAL] = "local",
  [SV_T_NOT_SET] = "not_in_use",
  [SV_T_PATHS] = "paths",
  [SV_T_SEARCH] = "search",
  [SV_T_SPECIAL] = "special",
  [SV_T_TARGET] = "targetitems",
};

static void sv_set_current_profile (scopedvar_t *sv, const char *name);
static void sv_set_comp_profile (scopedvar_t *sv, const char *name, mkc_compiler_t compiler);
static void sv_free_variables (sv_proflist_t *variables, bool hierarchyflag);
static sv_profile_t * sv_create (scopedvar_t *sv, sv_type_t svtype, const char *name, bool template);
static sv_profile_t * sv_create_profile (scopedvar_t *scope, sv_type_t svtype, const char *name);

static void sv_get_variable_str (scopedvar_t *scope, value_t *value, char *buff, size_t sz);
static void sv_sub_escapes (char *buff, size_t blen);
static int32_t sv_get_variable_integer (scopedvar_t *scope, value_t *value);
static value_t * sv_get_variable_value (scopedvar_t *scope, const char *str);

static void sv_proflist_init (sv_proflist_t *svlist);
static int sv_locate_svtype (scopedvar_t *sv, sv_type_t svtype);
static void sv_profile_check_create (scopedvar_t *sv, const char *name);
static void sv_compiler_check_create (scopedvar_t *sv, const char *name, mkc_compiler_t compiler);
static void sv_free_vars (scopedvar_t *sv);
static void sv_init_vars (scopedvar_t *sv, mkc_option_t *mkcoptions);
static void sv_push_hierarchy (scopedvar_t *sv, sv_profile_t *svprof);
static const char * sv_get_active_name (scopedvar_t *sv);
static varlist_t     *sv_get_varlist (scopedvar_t *sv, sv_type_t svtype, const char *vname);

scopedvar_t *
sv_init (mkc_log_t *log, mkc_error_t *mkcerr, mkc_option_t *mkcoptions)
{
  scopedvar_t   *sv;

  sv = malloc (sizeof (scopedvar_t));
  if (sv == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  sv->mkcoptions = mkcoptions;
  sv->mkcerr = mkcerr;
  sv->log = log;
  sv_proflist_init (&sv->profiles);
  sv->local_id = 0;
  sv->dfltcompiler = MKC_COMPILER_C;
  sv->currcompiler = MKC_COMPILER_GENERAL;
  sv->fromcache = false;
  sv->current_profile = MKC_C_PROF_NAME_DEFAULT;
  sv->active_prof = NULL;
  sv->active_idx = -1;
  sv->dfltprof_idx = -1;
  sv->currprof_idx = -1;
  sv->comp_idx = -1;

  sv_proflist_init (&sv->hierarchy);

  sv_init_vars (sv, mkcoptions);

  return sv;
}

void
sv_free (scopedvar_t *sv)
{
  if (sv == NULL) {
    return;
  }
  sv_free_vars (sv);
  sv_free_variables (&sv->hierarchy, true);
  free (sv);
}

void
sv_reset (scopedvar_t *sv, mkc_option_t *mkcoptions)
{
  if (sv == NULL) {
    return;
  }
  sv_free_vars (sv);
  sv_free_variables (&sv->hierarchy, true);
  sv_init_vars (sv, mkcoptions);
}

/* only local and target types are pushed */
void
sv_push (scopedvar_t *sv, sv_type_t svtype, const char *name)
{
  sv_profile_t   * svprof;

  if (svtype != SV_T_LOCAL &&
     svtype != SV_T_TARGET) {
    return;
  }

  svprof = sv_create (sv, svtype, name, false);
  if (svprof != NULL) {
    mkc_message (MKC_V_TMI, "push profile %s (%s)\n", svprof->name, svtypenames [svtype]);
    sv_push_hierarchy (sv, svprof);
  }
}

void
sv_pop (scopedvar_t *sv)
{
  sv_proflist_t   * proflist;
  sv_profile_t    * svprof;

  if (sv == NULL) {
    return;
  }

  proflist = &sv->hierarchy;
  if (proflist->sz <= 0) {
    mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, "scope");
    return;
  }

  svprof = &proflist->variables [proflist->sz - 1];
  mkc_message (MKC_V_TMI, "pop profile %s (%s)\n", svprof->name, svtypenames [svprof->svtype]);

  /* the standard scopes should never get popped off of the stack */
  proflist = &sv->hierarchy;
  if (proflist->sz == sv->standardsz) {
    mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, "scope-b");
    return;
  }

  proflist = &sv->profiles;
  proflist->sz -= 1;
  svprof = &proflist->variables [proflist->sz];
  datafree (svprof->name);
  varlist_free (svprof->varlist);
  svprof->varlist = NULL;
  svprof->svtype = SV_T_NOT_SET;

  proflist = &sv->hierarchy;
  proflist->sz -= 1;
  svprof = &proflist->variables [proflist->sz];
  svprof->svtype = SV_T_NOT_SET;
}

void
sv_set_default_compiler (scopedvar_t *sv, mkc_compiler_t compiler)
{
  const char  *active_name;

  if (sv == NULL) {
    return;
  }

  sv->dfltcompiler = compiler;
  sv_compiler_check_create (sv, MKC_C_PROF_NAME_DEFAULT, compiler);
  sv_compiler_check_create (sv, sv->current_profile, compiler);
  active_name = sv_get_active_name (sv);
  sv_set_comp_profile (sv, active_name, compiler);
  /* reset the active profile */
  sv_set_active_profile (sv, active_name);
}

void
sv_set_current_compiler (scopedvar_t *sv, mkc_compiler_t compiler)
{
  const char    * active_name;

  if (sv == NULL) {
    return;
  }

  sv->currcompiler = compiler;
  if (compiler == MKC_COMPILER_GENERAL) {
    /* nothing to do */
    return;
  }

  sv_compiler_check_create (sv, MKC_C_PROF_NAME_DEFAULT, compiler);
  active_name = sv_get_active_name (sv);
  sv_compiler_check_create (sv, active_name, compiler);
  sv_set_comp_profile (sv, active_name, compiler);
  sv->active_prof =
      &sv->hierarchy.variables [sv->comp_idx];
  sv->active_idx = sv->comp_idx;
}

void
sv_set_fromcache (scopedvar_t *sv, bool flag)
{
  if (sv == NULL) {
    return;
  }

  if (sv->fromcache != flag) {
    mkc_message (MKC_V_TMI, "set from-cache: %d\n", flag);
  }
  sv->fromcache = flag;
}

/* profile handling */

void
sv_incr_local_id (scopedvar_t *sv)
{
  sv->local_id += 1;
}

void
sv_decr_local_id (scopedvar_t *sv)
{
  sv_profile_t *svprof;
  int             sz;

  sz = sv->hierarchy.sz - 1;
  svprof = &sv->hierarchy.variables [sz];

  if ((svprof->svtype == SV_T_LOCAL ||
      svprof->svtype == SV_T_TARGET) &&
      svprof->local_id == sv->local_id) {
    sv_pop (sv);
  }

  sv->local_id -= 1;
  if (sv->local_id < 0) {
    mkc_error_set (sv->mkcerr, MKC_ERR_FATAL_ERROR, 0, "local-counter");
  }
}

/* the profile may be 'internal', 'default' or any other name */
/* when the cache is being loaded, or the profile is one of the */
/* namespaces, the active profile will point into the .profiles array */
void
sv_set_active_profile (scopedvar_t *sv, const char *name)
{
  int     idx = -1;

  /* when loading from the cache, there can be any sort of name */
  /* make sure the profile exists */
  sv_profile_check_create (sv, name);

  /* locate the name in the hierarchy */
  for (int i = sv->hierarchy.sz - 1; i >= 0; --i) {
    sv_profile_t    *svprof;

    svprof = &sv->hierarchy.variables [i];
    if (svprof->svtype != SV_T_CURR_PROF_COMPILER &&
        strcmp (svprof->name, name) == 0) {
      idx = i;
      sv->active_prof = svprof;
      sv->active_idx = idx;
      break;
    }
  }

  if (idx == -1) {
    /* try the profile list -- this happens when loading the cache */
    /* locate the name in the hierarchy */
    for (int i = sv->profiles.sz - 1; i >= 0; --i) {
      sv_profile_t    *svprof;

      svprof = &sv->profiles.variables [i];
      if (svprof->svtype != SV_T_CURR_PROF_COMPILER &&
          strcmp (svprof->name, name) == 0) {
        idx = i;
        sv->active_prof = svprof;
        break;
      }
    }
  }

  if (idx == -1) {
    mkc_log (sv->log, MKC_LOG_ERROR, "  scope-set-active: %s not found\n", name);
    return;
  }
}

const char *
sv_get_current_profile (scopedvar_t *sv)
{
  return sv->current_profile;
}

void
sv_reset_profile (scopedvar_t *sv)
{
  sv_set_active_profile (sv, MKC_C_PROF_NAME_DEFAULT);
  sv->currcompiler = sv->dfltcompiler;
}

/* iterators */

/* iterates over the profiles */
sv_iter_t *
sv_iter_start (scopedvar_t *sv, sv_iter_flag_t flags)
{
  sv_iter_t   *sviter;

  sviter = malloc (sizeof (sv_iter_t));
  if (sviter == NULL) {
    mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  sviter->idx = MKC_ITER_FINISH;
  sviter->flags = flags;
  if ((flags & SV_ITER_HIERARCHY) == SV_ITER_HIERARCHY) {
    sviter->profiles = &sv->hierarchy;
  }
  if ((flags & SV_ITER_PROFILES) == SV_ITER_PROFILES) {
    sviter->profiles = &sv->profiles;
  }

  return sviter;
}

/* iterates over the profiles */
const char *
sv_iter_next (scopedvar_t *sv, sv_iter_t *sviter)
{
  sv_profile_t    * svprof;

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
    return sv_iter_next (sv, sviter);
  }

  svprof = &sviter->profiles->variables [sviter->idx];
  if ((sviter->flags & SV_ITER_HIERARCHY) == SV_ITER_HIERARCHY) {
    if (sviter->idx == sv->currprof_idx) {
      if (svprof->svtype == SV_T_DFLT_PROF) {
        return sv_iter_next (sv, sviter);
      }
    }
    if (sviter->idx == sv->comp_idx) {
      if (svprof->compiler == MKC_COMPILER_GENERAL) {
        return sv_iter_next (sv, sviter);
      }
    }
  }

  return svprof->name;
}

void
sv_iter_finish (sv_iter_t *sviter)
{
  if (sviter == NULL) {
    return;
  }

  free (sviter);
}

sv_type_t
sv_iter_get_type (scopedvar_t *sv, sv_iter_t *sviter)
{
  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return SV_T_NOT_SET;
  }

  return sviter->profiles->variables [sviter->idx].svtype;
}

mkc_compiler_t
sv_iter_get_compiler (scopedvar_t *sv, sv_iter_t *sviter)
{
  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return MKC_COMPILER_GENERAL;
  }

  return sviter->profiles->variables [sviter->idx].compiler;
}

void
sv_var_iter_start (scopedvar_t *sv, sv_iter_t *sviter,
    mkc_varidx_t *variteridx)
{
  sv_profile_t    * svprof;
  varlist_t       * varlist = NULL;

  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return;
  }

  svprof = &sviter->profiles->variables [sviter->idx];
  varlist = svprof->varlist;
  var_iter_start (varlist, variteridx);

  return;
}

int
sv_var_iter_next (scopedvar_t *sv, sv_iter_t *sviter,
    mkc_varidx_t *variteridx)
{
  sv_profile_t    * svprof;
  varlist_t       * varlist = NULL;
  mkc_varidx_t    vidx;

  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return MKC_COMPILER_GENERAL;
  }

  svprof = &sviter->profiles->variables [sviter->idx];
  varlist = svprof->varlist;
  vidx = var_iter_next (varlist, variteridx);

  return vidx;
}

const char *
sv_var_iter_get_name (scopedvar_t *sv, sv_iter_t *sviter,
    mkc_varidx_t vidx)
{
  sv_profile_t * svprof;
  varlist_t       * varlist = NULL;
  const char      * vname;

  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return NULL;
  }

  svprof = &sviter->profiles->variables [sviter->idx];
  varlist = svprof->varlist;
  vname = var_get_name (varlist, vidx);

  return vname;
}

value_t *
sv_var_iter_get_value (scopedvar_t *sv, sv_iter_t *sviter,
    mkc_varidx_t vidx)
{
  sv_profile_t * svprof;
  varlist_t       * varlist = NULL;
  value_t         * value = NULL;

  if (sviter->idx < 0 || sviter->idx >= sviter->profiles->sz) {
    mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return NULL;
  }

  svprof = &sviter->profiles->variables [sviter->idx];
  varlist = svprof->varlist;
  value = var_get_value_by_idx (varlist, vidx);

  return value;
}

/* get */

int64_t
sv_get_timestamp (scopedvar_t *sv, sv_type_t svtype,
    const char *vname)
{
  value_t     *value;
  const char  *tag = NULL;

  if (svtype == SV_T_BUILD_DATA) {
    tag = MKC_C_BVAR_TIMESTAMP;
  }
  value = sv_get_value (sv, svtype, vname, tag);
  return sv_value_get_timestamp (sv, value);
}

value_t *
sv_get_value (scopedvar_t *sv, sv_type_t svtype,
    const char *vname, const char *tag)
{
  sv_profile_t    * svprof;
  value_t         * value = NULL;

  if (svtype == SV_T_ACTIVE) {
    svprof = sv->active_prof;
    svtype = svprof->svtype;
  }

  /* handle special type to variable mappings */
  if (svtype == SV_T_BUILD_DATA) {
    value_t   * dictval;

    dictval = sv_get_value (sv, SV_T_INTERNAL, MKC_C_VAR_BUILD_DATA, NULL);
    if (dictval == NULL || dictval->vtype != MKC_VT_DICT) {
      return NULL;
    }
    value = dictdict_get (dictval->dict, vname, tag, sv->mkcerr);
    return value;
  }
  if (svtype == SV_T_PATHS) {
    value_t   * dictval;

    dictval = sv_get_value (sv, SV_T_INTERNAL, MKC_C_VAR_BUILD_PATHS, NULL);
    if (dictval == NULL || dictval->vtype != MKC_VT_DICT) {
      return NULL;
    }
    value = dict_get (dictval->dict, vname);
    return value;
  }

  for (int i = sv->hierarchy.sz - 1; i >= 0; --i) {
    varlist_t       *varlist;

    svprof = &sv->hierarchy.variables [i];
    if (svtype != SV_T_SEARCH && svprof->svtype != svtype) {
      /* if a particular scope is selected */
      continue;
    }

    varlist = svprof->varlist;
    value = var_get_value (varlist, vname);
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
sv_value_get_integer (scopedvar_t *sv, value_t *value)
{
  int32_t       ival = 0;

  if (value == NULL) {
    mkc_error_set (sv->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return 0;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
      break;
    }
    case MKC_VT_RANGE:
    case MKC_VT_TIMESTAMP: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_INTEGER: {
      ival = value->ival;
      break;
    }
    case MKC_VT_LIST: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      ival = 0;
      break;
    }
    case MKC_VT_DICT: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
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
      ival = sv_get_variable_integer (sv, value);
      break;
    }
    case MKC_VT_STRING:
    case MKC_VT_STATIC_STRING:
    case MKC_VT_QUOTED_STRING: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
  }

  mkc_log (sv->log, MKC_LOG_PROCESS, "  scope-get-int: %" PRId32 "\n", ival);
  return ival;
}

int64_t
sv_value_get_timestamp (scopedvar_t *sv, value_t *value)
{
  int64_t    tmval = 0;

  if (value == NULL) {
    mkc_error_set (sv->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return 0;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_RANGE: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_TIMESTAMP: {
      tmval = value->tmval;
      break;
    }
    case MKC_VT_INTEGER:
    case MKC_VT_DICT:
    case MKC_VT_LIST: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
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

      tvalue = sv_get_variable_value (sv, value->sval);
      if (tvalue == NULL) {
        mkc_error_set (sv->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
        return 0;
      }
      if (tvalue->vtype == MKC_VT_TIMESTAMP) {
        tmval = tvalue->tmval;
      } else {
        mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      }
      break;
    }
    case MKC_VT_STRING:
    case MKC_VT_STATIC_STRING:
    case MKC_VT_QUOTED_STRING: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
  }

  mkc_log (sv->log, MKC_LOG_PROCESS, "  pv-get-int: %" PRId64 "\n", tmval);
  return tmval;
}

void
sv_value_get_str (scopedvar_t *sv, value_t *value,
    char *buff, size_t sz)
{
  *buff = '\0';

  if (value == NULL) {
    mkc_error_set (sv->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
      break;
    }
    case MKC_VT_RANGE: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
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

      tbuff = sv_substitute (sv, value->sval, SV_SUB_ESCAPE, 0);
      stpecpy (buff, buff + sz, tbuff);
      free (tbuff);
      break;
    }
    case MKC_VT_DICT:
    case MKC_VT_LIST: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_ENV_VARIABLE: {
      env_get (value->sval, buff, sz);
      break;
    }
    case MKC_VT_VARIABLE: {
      sv_get_variable_str (sv, value, buff, sz);
      break;
    }
  }

  mkc_log (sv->log, MKC_LOG_PROCESS, "  scope-get-str: %s\n", buff);
}

/* get the actual value of a value */
/* this is an issue for env-variables, quoted strings, lists and dicts */
value_t *
sv_value_get_value (scopedvar_t *sv, value_t *value, value_t *rvalue)
{
  value_t   * nvalue;

  /* in many cases the value returned is simply the value passed in */
  nvalue = value;

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
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
        mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
        return nvalue;
      }

      /* need to get the actual value */
      sv_value_get_str (sv, value, buff, MKC_PATH_MAX);

      value_init (rvalue);
      rvalue->isallocated = true;
      rvalue->vtype = MKC_VT_STRING;
      rvalue->vctxt = value->vctxt;
      rvalue->sval = buff;
      nvalue = rvalue;
      break;
    }
    case MKC_VT_VARIABLE: {
      nvalue = sv_get_variable_value (sv, value->sval);
      break;
    }
    case MKC_VT_DICT: {
      listidx_t     iteridx;
      dict_t        *ndict;
      dictitem_t    *ditem;

      /* each value in a dict must be processed */

      ndict = dict_init (sv->log, sv_value_free, sizeof (value_t), sv->mkcerr);

      dict_iter_start (value->dict, &iteridx);
      while ((ditem = dict_iter_next (value->dict, &iteridx)) != NULL) {
        const char  * name;
        value_t     * dvalue;
        value_t     tmpvalue;

        if (mkc_error_chk_err (sv->mkcerr)) {
          break;
        }

        name = dict_iter_get_name (ditem);
        dvalue = dict_iter_get_data (ditem);
        sv_value_get_value (sv, dvalue, &tmpvalue);
        dict_set (ndict, name, &tmpvalue);
      }

      value_init (rvalue);
      rvalue->isallocated = true;
      rvalue->vtype = MKC_VT_DICT;
      rvalue->vctxt = value->vctxt;
      rvalue->dict = ndict;
      nvalue = rvalue;
      break;
    }
    case MKC_VT_LIST: {
      listidx_t     iteridx;
      listidx_t     lidx;
      list_t        * nlist;

      /* each value in a list must be processed, as the value in the list */
      /* may be an env-variable or a quoted string or a list */
      /* the list may not need substitution, but just create a new list */
      /* in all cases */

      nlist = list_init (MKC_LIST_UNSORTED, sv_value_free, NULL,
          sizeof (value_t), sv->mkcerr);

      list_iter_start (value->list, &iteridx);
      while ((lidx = list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
        value_t   * lvalue;
        value_t   tmpvalue;

        if (mkc_error_chk_err (sv->mkcerr)) {
          break;
        }

        lvalue = list_get_by_idx (value->list, lidx);
        sv_value_get_value (sv, lvalue, &tmpvalue);
        list_set (nlist, &tmpvalue);
      }

      value_init (rvalue);
      rvalue->isallocated = true;
      rvalue->vtype = MKC_VT_LIST;
      rvalue->vctxt = value->vctxt;
      rvalue->list = nlist;
      nvalue = rvalue;
      break;
    }
  }

  if (nvalue != rvalue) {
    memcpy (rvalue, nvalue, sizeof (value_t));
  }

  return nvalue;
}

value_t *
sv_value_get_list_value (scopedvar_t *sv, value_t *value)
{
  value_t    *rvalue = NULL;

  if (value == NULL) {
    mkc_error_set (sv->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
      break;
    }
    case MKC_VT_ENV_VARIABLE:
    case MKC_VT_INTEGER:
    case MKC_VT_QUOTED_STRING:
    case MKC_VT_STATIC_STRING:
    case MKC_VT_STRING:
    case MKC_VT_TIMESTAMP: {
      mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_DICT:
    case MKC_VT_LIST: {
      rvalue = value;
      break;
    }
    case MKC_VT_RANGE: {
      rvalue = value;
      break;
    }
    case MKC_VT_VARIABLE: {
      value = sv_get_variable_value (sv, value->sval);
      if (value->vtype == MKC_VT_DICT ||
          value->vtype == MKC_VT_LIST) {
        rvalue = value;
      } else {
        mkc_error_set (sv->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      }
      break;
    }
  }

  return rvalue;
}

/* set */

void
sv_set_context (scopedvar_t *sv, const char *vname,
    value_ctxt_t vctxt)
{
  value_t   *value;

  if (sv == NULL || vname == NULL) {
    return;
  }

  value = sv_get_value (sv, SV_T_SEARCH, vname, NULL);
  if (value != NULL) {
    value->vctxt = vctxt;
  }
}

int
sv_set (scopedvar_t *sv, sv_type_t svtype,
    const char *vname, const char *tag, value_t *value, value_ctxt_t vctxt)
{
  varlist_t       *varlist = NULL;
  int             rc = MKC_ERR_FAILURE;

  if (sv == NULL) {
    return rc;
  }
  if (vname == NULL || value == NULL) {
    mkc_error_set (sv->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return rc;
  }

  if (mkc_msg_check_level (MKC_V_TMI)) {
    char    tmp [80];

    value_to_str (value, tmp, sizeof (tmp), 0);
    mkc_message (MKC_V_TMI, "set %s %s (%s): %s\n",
        vname, tag == NULL ? "" : tag, svtypenames [svtype], tmp);
  }

  /* handle special dictionary types */
  if (svtype == SV_T_BUILD_DATA) {
    value_t   * dictval;
    value_t   * tvalue;

    dictval = sv_get_value (sv, SV_T_INTERNAL, MKC_C_VAR_BUILD_DATA, NULL);
    if (dictval == NULL || dictval->vtype != MKC_VT_DICT) {
      mkc_error_set (sv->mkcerr, MKC_ERR_FATAL_ERROR, 0, "missing dict");
      return rc;
    }

    tvalue = malloc (sizeof (value_t));
    if (tvalue == NULL) {
      mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return rc;
    }
    value_copy (tvalue, value, sv->mkcerr);
    tvalue->isallocated = true;
    dictdict_set (dictval->dict, vname, tag, tvalue,
        sv->log, value_free, sv->mkcerr);
    free (tvalue);

    return MKC_OK;
  }
  if (svtype == SV_T_PATHS) {
    value_t   * dictval;

    dictval = sv_get_value (sv, SV_T_INTERNAL, MKC_C_VAR_BUILD_PATHS, NULL);
    if (dictval != NULL && dictval->vtype == MKC_VT_DICT) {
      value_t   * tvalue;

      tvalue = value;
      if (! sv->fromcache) {
        tvalue = malloc (sizeof (value_t));
        if (tvalue == NULL) {
          mkc_error_set (sv->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
          return rc;
        }
        value_copy (tvalue, value, sv->mkcerr);
        tvalue->isallocated = true;
      }
      dict_set (dictval->dict, vname, tvalue);
      free (tvalue);
    }
    return MKC_OK;
  }

  varlist = sv_get_varlist (sv, svtype, vname);
  if (varlist == NULL) {
    return rc;
  }

  value->vctxt = vctxt;
  var_set_fromcache (varlist, sv->fromcache);
  rc = var_set (varlist, vname, value);

  return MKC_OK;
}

int
sv_set_integer (scopedvar_t *sv, sv_type_t svtype,
    const char *vname, const char *tag, int32_t ival, value_ctxt_t vctxt)
{
  int       rc = MKC_ERR_FAILURE;
  value_t   value;

  value_init (&value);
  value.ival = ival;
  value.vtype = MKC_VT_INTEGER;

  rc = sv_set (sv, svtype, vname, tag, &value, vctxt);
  return rc;
}

int
sv_set_timestamp (scopedvar_t *sv, sv_type_t svtype,
    const char *vname, const char *tag, int64_t tmval, value_ctxt_t vctxt)
{
  int       rc = MKC_ERR_FAILURE;
  value_t   value;

  value_init (&value);
  value.tmval = tmval;
  value.vtype = MKC_VT_TIMESTAMP;

  rc = sv_set (sv, svtype, vname, tag, &value, vctxt);
  return rc;
}

int
sv_set_str (scopedvar_t *sv, sv_type_t svtype,
    const char *vname, const char *tag, const char *str, value_ctxt_t vctxt)
{
  int       rc = MKC_ERR_FAILURE;
  value_t   value;

  value_init (&value);
  value.sval = (char *) str;
  value.vtype = MKC_VT_STRING;

  rc = sv_set (sv, svtype, vname, tag, &value, vctxt);
  return rc;
}

int
sv_set_list (scopedvar_t *sv, sv_type_t svtype,
    const char *vname, const char *tag,
    list_t *list, value_ctxt_t vctxt)
{
  int       rc = MKC_ERR_FAILURE;
  value_t   value;

  value_init (&value);
  value.list = list;
  value.vtype = MKC_VT_LIST;

  rc = sv_set (sv, svtype, vname, tag, &value, vctxt);
  return rc;
}

int
sv_set_list_from_str (scopedvar_t *sv,
    const char *vname, char *str, value_ctxt_t vctxt)
{
  int           rc = MKC_ERR_FAILURE;
  char          *p;
  char          *tokstr;

  p = str_token (str, " ", &tokstr);
  while (p != NULL) {
    if (mkc_error_chk_err (sv->mkcerr)) {
      return MKC_ERR_FAILURE;
    }

    str_trim (p, 0);
    sv_append_str_list (sv, SV_T_SEARCH, vname, NULL, p, vctxt);
    p = str_token (NULL, " ", &tokstr);
  }

  return rc;
}

/* will create the value/list if it does not exist */
/* directly append the string to a value containing a list */
/* this is called using a known list value, so there are no */
/* verification checks */
int
sv_append_str_list (scopedvar_t *sv, sv_type_t svtype,
    const char *vname, const char *tag,
    const char *data, value_ctxt_t vctxt)
{
  value_t       *listval;
  list_t    *list;
  value_t       tvalue;


  listval = sv_get_value (sv, svtype, vname, tag);
  if (listval == NULL) {
    list = list_init (MKC_LIST_UNSORTED, value_free, NULL,
        sizeof (value_t), sv->mkcerr);
    sv_set_list (sv, svtype, vname, tag, list, vctxt);
    list_free (list);
    listval = sv_get_value (sv, svtype, vname, tag);
  }
  list = listval->list;

  if (data != NULL) {
    value_init (&tvalue);
    tvalue.vtype = MKC_VT_STRING;
    tvalue.sval = strdup (data);
    list_set (list, &tvalue);
  }

  return MKC_OK;
}

int
sv_set_dict (scopedvar_t *sv, sv_type_t svtype,
    const char *vname, dict_t *dict, value_ctxt_t vctxt)
{
  int       rc = MKC_ERR_FAILURE;
  value_t   value;

  value_init (&value);
  value.dict = dict;
  value.vtype = MKC_VT_DICT;

  rc = sv_set (sv, svtype, vname, NULL, &value, vctxt);
  return rc;
}

void
sv_delete (scopedvar_t *sv, sv_type_t svtype,
    const char *vname, const char * tag)
{
  varlist_t       *varlist;

  if (sv == NULL) {
    return;
  }
  if (vname == NULL) {
    mkc_error_set (sv->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  if (svtype == SV_T_BUILD_DATA) {
    value_t   * dictval;

    if (tag == NULL) {
      mkc_error_set (sv->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
      return;
    }
    dictval = sv_get_value (sv, SV_T_INTERNAL, MKC_C_VAR_BUILD_DATA, NULL);
    if (dictval == NULL) {
      return;
    }
    dictdict_delete (dictval->dict, vname, tag, sv->mkcerr);
  } else {
    varlist = sv_get_varlist (sv, svtype, vname);
    if (varlist == NULL) {
      return;
    }

    var_delete (varlist, vname);
  }
}

bool
sv_is_defined (scopedvar_t *sv, sv_type_t svtype,
    const char *vname, const char *tag)
{
  value_t     *value;

  if (sv == NULL) {
    return false;
  }

  value = sv_get_value (sv, svtype, vname, tag);
  if (value == NULL) {
    return false;
  }
  return true;
}

bool
sv_var_is_dict (scopedvar_t *sv, const char *vname)
{
  value_t     *value;
  bool        rc = false;

  if (sv == NULL) {
    return rc;
  }

  value = sv_get_value (sv, SV_T_SEARCH, vname, NULL);
  if (value == NULL) {
    return rc;
  }

  if (value->vtype == MKC_VT_DICT) {
    rc = true;
  }
  return rc;
}

bool
sv_var_is_list (scopedvar_t *sv, const char *vname)
{
  value_t     *value;
  bool        rc = false;

  if (sv == NULL) {
    return rc;
  }

  value = sv_get_value (sv, SV_T_SEARCH, vname, NULL);
  if (value == NULL) {
    return rc;
  }

  if (value->vtype == MKC_VT_LIST || value->vtype == MKC_VT_RANGE) {
    rc = true;
  }
  return rc;
}

/* only frees the value if the value was allocated */
void
sv_value_free (void *tvalue)
{
  value_t   *value = tvalue;

  if (value == NULL) {
    return;
  }

  if (value->isallocated) {
    value_free (value);
  }
}

/* processes the internal substitutions */
/* if the string is a variable, the final substitution is done */
/* by the caller by calling sv_get_value () */
char *
sv_substitute (scopedvar_t *sv, const char *data,
    sv_escape_t subescapeflag, int depth)
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

  if (sv == NULL) {
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
      sv_sub_escapes (buff, blen);
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
        mkc_error_set (sv->mkcerr, MKC_ERR_UNBALANCED_BRACES, 0, NULL);
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
      tstr = sv_substitute (sv, substr, SV_NO_ESCAPE, depth + 1);
      free (substr);
//fprintf (stderr, "%*ststr: '%s'\n", depth * 2, "", tstr);

      if (isenv) {
        env_get (tstr, ebuff, sizeof (ebuff));
        tval = ebuff;
      } else {
        value_t   *value;

        value = sv_get_value (sv, SV_T_SEARCH, tstr, NULL);
//fprintf (stderr, "%*svalue-null? %d\n", depth * 2, "", value == NULL ? 1 : 0);
        if (value != NULL && value->vtype == MKC_VT_INTEGER) {
          snprintf (tbuff, sizeof (tbuff), "%" PRId32, value->ival);
          tval = tbuff;
        }
        if (value != NULL && value->vtype == MKC_VT_STRING) {
          tval = value->sval;
        }
//fprintf (stderr, "%*s got %s\n", depth * 2, "", tval);
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
    sv_sub_escapes (buff, blen);
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
sv_set_current_profile (scopedvar_t *sv, const char *name)
{
  sv_profile_t      *svprof = NULL;
  sv_profile_t      *fsvprof = NULL;

  for (int i = 0; i < sv->profiles.sz; ++i) {
    svprof = &sv->profiles.variables [i];

    if (svprof->svtype != SV_T_CURR_PROF_COMPILER &&
        strcmp (svprof->name, name) == 0) {
      fsvprof = svprof;
      break;
    }
  }

  if (fsvprof != NULL) {
    sv_profile_t    * hsvprof;

    hsvprof = &sv->hierarchy.variables [sv->currprof_idx];
    memcpy (hsvprof, fsvprof, sizeof (sv_profile_t));
  } else {
    mkc_error_set (sv->mkcerr, MKC_ERR_FATAL_ERROR, 0, "profile not found");
    fprintf (stderr, "ERR: set-curr-profile: profile %s not found\n", name);
  }
}

static void
sv_set_comp_profile (scopedvar_t *sv, const char *name,
    mkc_compiler_t compiler)
{
  sv_profile_t      *svprof = NULL;
  sv_profile_t      *fsvprof = NULL;
  sv_type_t         searchtype = SV_T_CURR_PROF_COMPILER;

  if (compiler == MKC_COMPILER_GENERAL) {
    sv->currcompiler = sv->dfltcompiler;
  }

  for (int i = 0; i < sv->profiles.sz; ++i) {
    svprof = &sv->profiles.variables [i];

    if (svprof->svtype == searchtype &&
        strcmp (svprof->name, name) == 0 &&
        svprof->compiler == sv->currcompiler) {
      fsvprof = svprof;
      break;
    }
  }

  if (fsvprof != NULL) {
    sv_profile_t    * hsvprof;

    hsvprof = &sv->hierarchy.variables [sv->comp_idx];
    memcpy (hsvprof, svprof, sizeof (sv_profile_t));
  }
}

static void
sv_free_variables (sv_proflist_t *profiles, bool hierarchyflag)
{
  if (profiles != NULL) {
    if (! hierarchyflag) {
      for (int i = 0; i < profiles->sz; ++i) {
        sv_profile_t   *svprof;

        svprof = &profiles->variables [i];
        datafree (svprof->name);
        varlist_free (svprof->varlist);
      }
    }
    free (profiles->variables);
  }
}

static sv_profile_t *
sv_create (scopedvar_t *sv, sv_type_t svtype,
    const char *name, bool template)
{
  sv_profile_t    * svprof;

  if (svtype == SV_T_LOCAL || svtype == SV_T_TARGET) {
    char    tbuff [80];

    /* when attempting to create a local scope, first check to see */
    /* if it already exists */
    for (int i = sv->profiles.sz - 1; i >= 0; --i) {
      sv_profile_t   *svprof;

      svprof = &sv->profiles.variables [i];
      if (svprof->svtype == svtype) {
        if (svtype == SV_T_LOCAL &&
            svprof->local_id == sv->local_id) {
          /* already exists */
          return NULL;
        }
        if (svtype == SV_T_TARGET) {
          return NULL;
        }
      }
    }

    if (svtype == SV_T_LOCAL) {
      snprintf (tbuff, sizeof (tbuff), "%s-%" PRId32, name, sv->local_id);
    } else {
      stpecpy (tbuff, tbuff + sizeof (tbuff), name);
    }
    svprof = sv_create_profile (sv, svtype, tbuff);
  } else {
    svprof = sv_create_profile (sv, svtype, name);
  }

  return svprof;
}

static sv_profile_t *
sv_create_profile (scopedvar_t *sv,
    sv_type_t svtype, const char *name)
{
  sv_proflist_t   * profiles;
  sv_profile_t    * svprof;

  profiles = &sv->profiles;
  if (profiles->sz >= profiles->allocsz) {
    profiles->allocsz += 10;
    profiles->variables = realloc (profiles->variables,
        sizeof (sv_profile_t) * profiles->allocsz);
    for (int i = profiles->sz; i < profiles->allocsz; ++i) {
      svprof = &profiles->variables [i];

      svprof->name = NULL;
      svprof->varlist = NULL;
      svprof->svtype = SV_T_NOT_SET;
      svprof->compiler = MKC_COMPILER_GENERAL;
      svprof->local_id = sv->local_id;
    }
  }

  svprof = &profiles->variables [profiles->sz];
  svprof->varlist = varlist_init (sv->log, sv->mkcerr);
  svprof->svtype = svtype;
  svprof->local_id = sv->local_id;
  if (svtype == SV_T_CURR_PROF_COMPILER) {
    svprof->compiler = sv->currcompiler;
  }

  if (name != NULL) {
    svprof->name = strdup (name);
  }
  profiles->sz += 1;

  return svprof;
}

static void
sv_get_variable_str (scopedvar_t *sv, value_t *value,
    char *buff, size_t sz)
{
  value_t     *tvalue;

  if (sv == NULL) {
    return;
  }

  *buff = '\0';

  tvalue = sv_get_variable_value (sv, value->sval);
  if (mkc_error_chk_err (sv->mkcerr)) {
    return;
  }
  if (tvalue == NULL) {
    mkc_error_set (sv->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
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

    mkc_log (sv->log, MKC_LOG_PROCESS, "  scope-get-var-str: %s\n",
        value_to_str (tvalue, dbuff, sizeof (dbuff), 0));
  }
}

static void
sv_sub_escapes (char *buff, size_t blen)
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
sv_get_variable_integer (scopedvar_t *sv, value_t *value)
{
  int32_t     ival = 0;
  value_t     *tvalue;

  tvalue = sv_get_variable_value (sv, value->sval);
  if (tvalue == NULL) {
    mkc_error_set (sv->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
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
sv_get_variable_value (scopedvar_t *sv, const char *str)
{
  char        *tstr;
  value_t     *value;

  tstr = sv_substitute (sv, str, SV_NO_ESCAPE, 0);
  if (tstr == NULL) {
    return NULL;
  }
  value = sv_get_value (sv, SV_T_SEARCH, tstr, NULL);
  free (tstr);
  return value;
}

static void
sv_proflist_init (sv_proflist_t *svlist)
{
  svlist->variables = NULL;
  svlist->allocsz = 0;
  svlist->sz = 0;
}

static int
sv_locate_svtype (scopedvar_t *sv, sv_type_t svtype)
{
  int     idx = -1;

  for (int i = sv->profiles.sz - 1; i >= 0; --i) {
    if (sv->profiles.variables [i].svtype == svtype) {
      idx = i;
      break;
    }
  }

  return idx;
}

static void
sv_profile_check_create (scopedvar_t *sv, const char *name)
{
  bool  found = false;

  /* check and see if this profile has already been created */
  for (int i = 0; i < sv->profiles.sz; ++i) {
    if (strcmp (sv->profiles.variables [i].name, name) == 0) {
      found = true;
      break;
    }
  }

  if (! found) {
    sv_create (sv, SV_T_CURR_PROF, name, false);
  }
}

static void
sv_compiler_check_create (scopedvar_t *sv,
    const char *name, mkc_compiler_t compiler)
{
  /* check and see if this compiler has already been created */
  for (int i = 0; i < sv->profiles.sz; ++i) {
    sv_profile_t    * svprof;

    svprof = &sv->profiles.variables [i];

    if (svprof->svtype == SV_T_CURR_PROF_COMPILER &&
        strcmp (svprof->name, name) == 0 &&
        svprof->compiler == compiler) {
      return;
    }
  }

  sv_create (sv, SV_T_CURR_PROF_COMPILER, name, false);
}

static void
sv_free_vars (scopedvar_t *sv)
{
  if (sv == NULL) {
    return;
  }
  sv_free_variables (&sv->profiles, false);
}

static void
sv_init_vars (scopedvar_t *sv, mkc_option_t *mkcoptions)
{
  sv_profile_t    * svprof;

  /* create the standard set of scopes */
  /* when searching for a variable, the scopes will be traversed */
  /* in reverse order */
  svprof = sv_create (sv, SV_T_INTERNAL, MKC_C_PROF_NAME_INTERNAL, false);
  sv_push_hierarchy (sv, svprof);

  sv->currcompiler = MKC_COMPILER_GENERAL;
  /* the default profile will hold most variables */
  /* this is useful, as the variables will be cached for */
  /* all of the different user profiles */
  svprof = sv_create (sv, SV_T_DFLT_PROF, MKC_C_PROF_NAME_DEFAULT, false);
  sv->dfltprof_idx = sv->hierarchy.sz;
  sv_push_hierarchy (sv, svprof);
  sv->currprof_idx = sv->hierarchy.sz;
  sv_push_hierarchy (sv, svprof);

  /* this is not a valid curr-prof-compiler, but it will be replaced */
  sv->comp_idx = sv->hierarchy.sz;
  sv_push_hierarchy (sv, svprof);

  /* the basic hierarchy is complete */
  sv->standardsz = sv->hierarchy.sz;

  sv->current_profile = mkcoptions->currprofile;
  sv->currcompiler = MKC_COMPILER_C;

  if (strcmp (sv->current_profile, MKC_C_PROF_NAME_DEFAULT) != 0) {
    sv_create (sv, SV_T_CURR_PROF, sv->current_profile, false);
  }

  sv_set_current_profile (sv, sv->current_profile);
  sv_set_comp_profile (sv, MKC_C_PROF_NAME_DEFAULT, sv->currcompiler);
  sv_set_active_profile (sv, MKC_C_PROF_NAME_DEFAULT);
}

static void
sv_push_hierarchy (scopedvar_t *sv, sv_profile_t *svprof)
{
  sv_proflist_t   * profiles;
  sv_profile_t    * hsvprof;

  profiles = &sv->hierarchy;
  if (profiles->sz >= profiles->allocsz) {
    profiles->allocsz += 10;
    profiles->variables = realloc (profiles->variables,
        sizeof (sv_profile_t) * profiles->allocsz);
    for (int i = profiles->sz; i < profiles->allocsz; ++i) {
      hsvprof = &profiles->variables [i];

      hsvprof->name = NULL;
      hsvprof->varlist = NULL;
      hsvprof->svtype = SV_T_NOT_SET;
      hsvprof->compiler = MKC_COMPILER_GENERAL;
      hsvprof->local_id = sv->local_id;
    }
  }

  hsvprof = &profiles->variables [profiles->sz];
  memcpy (hsvprof, svprof, sizeof (sv_profile_t));
  profiles->sz += 1;
}

static const char *
sv_get_active_name (scopedvar_t *sv)
{
  return sv->active_prof->name;
}

/* used for sv_set() */
static varlist_t     *
sv_get_varlist (scopedvar_t *sv, sv_type_t svtype, const char *vname)
{
  varlist_t       *varlist = NULL;
  int             idx = -1;
  sv_profile_t    * svprof = NULL;


  if (svtype == SV_T_ACTIVE) {
    sv_profile_t * svprof;

    svprof = sv->active_prof;
    svtype = svprof->svtype;
  }

  if (svtype == SV_T_SEARCH) {
    /* search any local scopes that are on the stack */
    /* if the active_idx is reached, stop there */
    for (int i = sv->hierarchy.sz - 1; i >= 0; --i) {
      svprof = &sv->hierarchy.variables [i];

      if (i == sv->active_idx) {
        idx = i;
        varlist = svprof->varlist;
        break;
      }

      if (svprof->svtype == SV_T_LOCAL) {
        varlist = svprof->varlist;
        if (var_is_defined (varlist, vname)) {
          idx = i;
          break;
        }
      }
    }
  } else {
    if (svtype == SV_T_LOCAL) {
      sv_push (sv, SV_T_LOCAL, "local");
    }
    /* the set statement is for a specific profile */
    idx = sv_locate_svtype (sv, svtype);
    svprof = &sv->profiles.variables [idx];
    varlist = svprof->varlist;
  }

  if (idx == -1) {
    return varlist;
  }

  return varlist;
}
