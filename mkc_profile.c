/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "mkc_compiler.h"
#include "mkc_const.h"
#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_option.h"
#include "mkc_profile.h"
#include "mkc_string.h"
#include "mkc_var.h"

enum {
  MKC_PROF_STACK_MAX = 20,
};

typedef struct mkc_prof_entry_t {
  mkc_varlist_t     * varlist;
  char              * name;
  mkc_compiler_t    compiler;
  mkc_prof_type_t   type;
  mkc_profidx_t     pidx;
} mkc_prof_entry_t;

typedef struct mkc_profile_t {
  mkc_list_t        * list;
  mkc_error_t       * mkcerr;
  mkc_log_t         * log;
  mkc_option_t      * mkcoptions;
  mkc_compiler_t    dfltcompiler;
  mkc_profidx_t     localstack [MKC_PROF_STACK_MAX];
  mkc_profidx_t     stack [MKC_PROF_STACK_MAX];
  mkc_profidx_t     active_idx;
  mkc_profidx_t     current_idx;
  int               stacksz;
  int               localstacksz;
} mkc_profile_t;

static void mkc_profile_entry_free (void *pentry);
static int mkc_profile_compare (void *tentrya, void *tentryb);

MKC_NODISCARD
mkc_profile_t *
mkc_profile_init (mkc_log_t *log, mkc_error_t *mkcerr, mkc_option_t *mkcoptions)
{
  mkc_profile_t   *profiles;
  mkc_profidx_t   pidx;

  profiles = malloc (sizeof (mkc_profile_t));
  if (profiles == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  profiles->stacksz = 0;
  profiles->localstacksz = 0;
  profiles->mkcerr = mkcerr;
  profiles->log = log;
  profiles->dfltcompiler = MKC_COMPILER_GENERAL;
  profiles->mkcoptions = mkcoptions;

  if (mkcoptions->compilertxt != NULL) {
    profiles->dfltcompiler = mkc_compiler_get (mkcoptions->compilertxt);
    if (profiles->dfltcompiler == MKC_COMPILER_UNKNOWN) {
      mkc_error_set (mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
      mkc_profile_free (profiles);
      return NULL;
    }
  }

  profiles->list = mkc_list_init (MKC_LIST_SORTED,
      mkc_profile_entry_free, mkc_profile_compare, mkcerr);
  if (profiles->list == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    mkc_profile_free (profiles);
    return NULL;
  }

  mkc_profile_create (profiles, MKC_C_PROF_INTERNAL_NAME,
      MKC_COMPILER_GENERAL, MKC_PROF_TYPE_INTERNAL);

  /* create local temporary variables */
  pidx = mkc_profile_create (profiles, MKC_C_PROF_TEMP_NAME,
        MKC_COMPILER_GENERAL, MKC_PROF_TYPE_TEMP);

  /* create current/compiler */
  pidx = mkc_profile_create (profiles, mkcoptions->dfltprofile,
        profiles->dfltcompiler, MKC_PROF_TYPE_CURRENT);

  /* create current/general */
  /* this will be the active profile */
  if (profiles->dfltcompiler != MKC_COMPILER_GENERAL) {
    pidx = mkc_profile_create (profiles, mkcoptions->dfltprofile,
        MKC_COMPILER_GENERAL, MKC_PROF_TYPE_CURRENT);
  }

  if (mkc_profile_get_type (profiles, pidx) != MKC_PROF_TYPE_CURRENT) {
    mkc_error_set (mkcerr, MKC_ERR_INVALID_PROFILE, 0, NULL);
    mkc_profile_free (profiles);
    return NULL;
  }

  mkc_message ("-- default profile: %s %s\n", mkcoptions->dfltprofile,
      mkc_compiler_get_name (profiles->dfltcompiler));
  mkc_log (log, MKC_LOG_PROFILE, "== default profile: %s %s\n",
      mkcoptions->dfltprofile, mkc_compiler_get_name (profiles->dfltcompiler));

  profiles->active_idx = pidx;
  profiles->current_idx = pidx;

  return profiles;
}

void
mkc_profile_free (mkc_profile_t *profiles)
{
  if (profiles == NULL) {
    return;
  }

  if (profiles->list != NULL) {
    mkc_list_free (profiles->list);
  }
  free (profiles);
}

int
mkc_profile_clear (mkc_profile_t *profiles, mkc_profidx_t pidx)
{
  mkc_prof_entry_t    *pentry;

  if (profiles == NULL) {
    return MKC_ERR_FAILURE;
  }

  if (pidx < 0 || pidx >= mkc_list_size (profiles->list)) {
    mkc_log (profiles->log, MKC_LOG_GENERAL,
        "profile: pidx: oor: %" PRId32 "\n", pidx);
    mkc_error_set (profiles->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  pentry = mkc_list_get_by_idx (profiles->list, pidx);

  if (pentry->varlist != NULL) {
    mkc_varlist_free (pentry->varlist);
  }
  pentry->varlist = mkc_varlist_init (profiles->log, profiles->mkcerr);
  if (pentry->varlist == NULL) {
    return MKC_ERR_FAILURE;
  }

  return MKC_OK;
}

mkc_compiler_t
mkc_profile_get_dflt_compiler (mkc_profile_t *profiles)
{
  if (profiles == NULL) {
    return MKC_COMPILER_UNKNOWN;
  }

  return profiles->dfltcompiler;
}

void
mkc_profile_set_dflt_compiler (mkc_profile_t *profiles,
    mkc_compiler_t compiler)
{
  if (profiles == NULL) {
    return;
  }

  profiles->dfltcompiler = compiler;
}

int
mkc_profile_create (mkc_profile_t *profiles, const char *pname,
    mkc_compiler_t compiler, mkc_prof_type_t type)
{
  mkc_prof_entry_t    *pentry;
  mkc_prof_entry_t    tentry;
  mkc_profidx_t       loc = MKC_LIST_NOTFOUND;

  if (profiles == NULL) {
    return MKC_ERR_FAILURE;
  }

  /* make sure the general category for this profile exists */
  if (compiler != MKC_COMPILER_GENERAL) {
    mkc_profidx_t   pidx;

    pidx = mkc_profile_find (profiles, pname, MKC_COMPILER_GENERAL);
    if (pidx == MKC_PROF_NOT_FOUND) {
      pidx = mkc_profile_create (profiles, pname,
          MKC_COMPILER_GENERAL, type);
    }
  }

  tentry.name = strdup (pname);
  if (tentry.name == NULL) {
    mkc_error_set (profiles->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }
  tentry.compiler = compiler;
  tentry.type = type;
  tentry.varlist = mkc_varlist_init (profiles->log, profiles->mkcerr);
  if (tentry.varlist == NULL) {
    return MKC_ERR_FAILURE;
  }

  pentry = mkc_list_set (profiles->list, &tentry,
      sizeof (mkc_prof_entry_t), &loc);
  if (pentry == NULL) {
    return MKC_ERR_FAILURE;
  }
  pentry->pidx = loc;

  mkc_log (profiles->log, MKC_LOG_PROFILE,
      "profile: create: %s %s (%" PRId32 ")\n", pname,
      mkc_compiler_get_name (compiler), loc);

  return loc;
}

int
mkc_profile_local_create (mkc_profile_t *profiles)
{
  mkc_prof_entry_t  *pentry;
  mkc_prof_entry_t  tentry;
  mkc_listidx_t     loc = MKC_LIST_NOTFOUND;
  char              tbuff [40];

  if (profiles == NULL) {
    return MKC_ERR_FAILURE;
  }

  if (profiles->stacksz >= MKC_PROF_STACK_MAX) {
    mkc_error_set (profiles->mkcerr, MKC_ERR_EXCEEDS_STACK_SIZE, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  snprintf (tbuff, sizeof (tbuff), "local_%d", profiles->localstacksz);

  tentry.name = strdup (tbuff);
  if (tentry.name == NULL) {
    mkc_error_set (profiles->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }
  tentry.compiler = MKC_COMPILER_GENERAL;
  tentry.type = MKC_PROF_TYPE_LOCAL;
  tentry.varlist = mkc_varlist_init (profiles->log, profiles->mkcerr);
  if (tentry.varlist == NULL) {
    return MKC_ERR_FAILURE;
  }

  /* local profiles are appended to the sorted list, and cannot */
  /* be found using the usual profile search */
  pentry = mkc_list_append (profiles->list, &tentry,
      sizeof (mkc_prof_entry_t), &loc);
  if (pentry == NULL) {
    mkc_error_set (profiles->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  mkc_log (profiles->log, MKC_LOG_PROFILE,
      "profile: create-local: %s (%" PRId32 ")\n", tbuff, loc);
  profiles->localstack [profiles->localstacksz] = loc;
  profiles->localstacksz += 1;


  return loc;
}

void
mkc_profile_local_pop (mkc_profile_t *profiles)
{
  mkc_prof_entry_t  *pentry;
  int               stackidx;

  if (profiles->localstacksz <= 0) {
    return;
  }

  profiles->localstacksz -= 1;
  stackidx = profiles->localstacksz;
  pentry = mkc_list_get_by_idx (profiles->list, profiles->localstack [stackidx]);

  mkc_log (profiles->log, MKC_LOG_PROFILE,
      "profile: pop-local: %s (%" PRId32 ")\n", pentry->name, pentry->pidx);
  mkc_list_pop (profiles->list, profiles->localstack [stackidx]);
}

mkc_profidx_t
mkc_profile_find (mkc_profile_t *profiles, const char *pname,
    mkc_compiler_t compiler)
{
  mkc_prof_entry_t  tentry;
  mkc_profidx_t     idx;
  mkc_listidx_t     loc = MKC_LIST_NOTFOUND;

  if (profiles == NULL) {
    return MKC_ERR_FAILURE;
  }

  tentry.name = (char *) pname;
  tentry.compiler = compiler;

  idx = mkc_list_find (profiles->list, &tentry, &loc);
  return idx;
}

mkc_varlist_t *
mkc_profile_get_varlist (mkc_profile_t *profiles, mkc_profidx_t pidx)
{
  mkc_prof_entry_t  *pentry;
  mkc_varlist_t    *varlist;

  if (profiles == NULL) {
    return NULL;
  }

  pentry = mkc_list_get_by_idx (profiles->list, pidx);
  if (pentry == NULL) {
    return NULL;
  }

  varlist = pentry->varlist;
  return varlist;
}

void
mkc_profile_iter_start (mkc_profile_t *profiles, mkc_profidx_t *iteridx)
{
  if (profiles == NULL || iteridx == NULL) {
    return;
  }

  mkc_list_iter_start (profiles->list, iteridx);
}

mkc_profidx_t
mkc_profile_iter_next (mkc_profile_t *profiles, mkc_profidx_t *iteridx)
{
  mkc_profidx_t   idx;

  if (profiles == NULL || iteridx == NULL) {
    return MKC_ITER_FINISH;
  }

  idx = mkc_list_iter_next (profiles->list, iteridx);
  return idx;
}

const char *
mkc_profile_get_name (mkc_profile_t *profiles, mkc_profidx_t pidx)
{
  mkc_prof_entry_t   *pentry;

  if (profiles == NULL) {
    return NULL;
  }

  if (pidx == MKC_PROF_NOT_FOUND) {
    return "not-found";
  }

  pentry = mkc_list_get_by_idx (profiles->list, pidx);
  if (pentry == NULL) {
    return NULL;
  }

  return pentry->name;
}

mkc_compiler_t
mkc_profile_get_compiler (mkc_profile_t *profiles, mkc_profidx_t pidx)
{
  mkc_prof_entry_t   *pentry;

  if (profiles == NULL) {
    return MKC_COMPILER_C;
  }

  if (pidx == MKC_PROF_NOT_FOUND) {
    return MKC_COMPILER_UNKNOWN;
  }

  pentry = mkc_list_get_by_idx (profiles->list, pidx);
  if (pentry == NULL) {
    return profiles->dfltcompiler;
  }

  return pentry->compiler;
}

const char *
mkc_profile_get_comp_name (mkc_profile_t *profiles, mkc_profidx_t pidx)
{
  mkc_compiler_t   compiler;

  compiler = mkc_profile_get_compiler (profiles, pidx);
  return mkc_compiler_get_name (compiler);
}

mkc_prof_type_t
mkc_profile_get_type (mkc_profile_t *profiles, mkc_profidx_t pidx)
{
  mkc_prof_entry_t   *pentry;

  if (profiles == NULL) {
    return MKC_PROF_TYPE_INVALID;
  }

  pentry = mkc_list_get_by_idx (profiles->list, pidx);
  if (pentry == NULL) {
    return MKC_PROF_TYPE_INVALID;
  }

  return pentry->type;
}

mkc_profidx_t
mkc_profile_push (mkc_profile_t *profiles)
{
  if (profiles == NULL) {
    return MKC_ERR_FAILURE;
  }

  if (profiles->stacksz >= MKC_PROF_STACK_MAX) {
    mkc_error_set (profiles->mkcerr, MKC_ERR_EXCEEDS_STACK_SIZE, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  profiles->stack [profiles->stacksz] = profiles->active_idx;
  profiles->stacksz += 1;

  mkc_log (profiles->log, MKC_LOG_PROFILE,
      "profile: push (%" PRId32 ")\n", profiles->active_idx);

  return profiles->active_idx;
}

mkc_profidx_t
mkc_profile_pop (mkc_profile_t *profiles)
{
  mkc_prof_entry_t    *pentry;

  if (profiles == NULL) {
    return MKC_ERR_FAILURE;
  }

  if (profiles->stacksz <= 0) {
    mkc_log (profiles->log, MKC_LOG_GENERAL,
        "profile: stacksz: oor: %" PRId32 "\n", profiles->stacksz);
    mkc_error_set (profiles->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  profiles->stacksz -= 1;
  profiles->active_idx = profiles->stack [profiles->stacksz];

  mkc_log (profiles->log, MKC_LOG_PROFILE,
      "profile: pop (%" PRId32 ")\n", profiles->active_idx);

  pentry = mkc_list_get_by_idx (profiles->list, profiles->active_idx);
  if (pentry->type == MKC_PROF_TYPE_CURRENT) {
    mkc_log (profiles->log, MKC_LOG_PROFILE,
        "profile: current: %s %s (%" PRId32 ")\n", pentry->name,
        mkc_compiler_get_name (pentry->compiler), profiles->active_idx);
    profiles->current_idx = profiles->active_idx;
  }

  return profiles->active_idx;
}

void
mkc_profile_set_active (mkc_profile_t *profiles, mkc_profidx_t pidx)
{
  mkc_prof_entry_t    *pentry;

  if (profiles == NULL) {
    return;
  }

  if (pidx < 0 ||
      pidx > mkc_list_size (profiles->list) + profiles->localstacksz) {
    mkc_log (profiles->log, MKC_LOG_GENERAL,
        "profile: pidx: oor: %" PRId32 "\n", pidx);
    mkc_error_set (profiles->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return;
  }

  profiles->active_idx = pidx;
  pentry = mkc_list_get_by_idx (profiles->list, pidx);
  if (pentry == NULL) {
    mkc_log (profiles->log, MKC_LOG_PROFILE,
        "profile: failed to find %" PRId32 "\n", pidx);
    return;
  }
  if (pentry->type == MKC_PROF_TYPE_CURRENT) {
    mkc_log (profiles->log, MKC_LOG_PROFILE,
        "profile: current: %s %s (%" PRId32 ")\n", pentry->name,
        mkc_compiler_get_name (pentry->compiler), pidx);
    profiles->current_idx = pidx;
  }

  mkc_log (profiles->log, MKC_LOG_PROFILE,
      "profile: active: %s %s (%" PRId32 ")\n",
      pentry->name, mkc_compiler_get_name (pentry->compiler), pidx);
}

mkc_profidx_t
mkc_profile_get_active (mkc_profile_t *profiles)
{
  if (profiles == NULL) {
    return 0;
  }

  return profiles->active_idx;
}

void
mkc_profile_iter_hierarchy_start (mkc_profile_t *profiles,
    mkc_profiter_t *profiter)
{
  mkc_profidx_t     pidx;
  mkc_prof_entry_t  * pentry;

  if (profiles == NULL || profiter == NULL) {
    return;
  }

  /* note that current active profile is one of */
  /* target/compiler, current/compiler, or current/general */
  pidx = mkc_profile_get_active (profiles);
  pentry = mkc_list_get_by_idx (profiles->list, pidx);

  profiter->pname = mkc_profile_get_name (profiles, profiles->current_idx);
  profiter->origpidx = pidx;
  profiter->ptype = pentry->type;
  profiter->pidx = MKC_PROF_NOT_FOUND;
  profiter->localidx = profiles->localstacksz - 1;
}

/* returns the next profile in the hierarchy (that exists) */
/*    local variable stack */
/*    target / compiler */
/*    temporary */
/*    current-user-profile / compiler */
/*    current-user-profile / general */
/*    internal */
int
mkc_profile_iter_hierarchy_next (mkc_profile_t *profiles,
    mkc_profiter_t *profiter)
{
  mkc_profidx_t     pidx;

  if (profiles == NULL || profiter == NULL) {
    return MKC_ERR_FAILURE;
  }

  if (profiter->localidx >= 0) {
    /* note that profiter->pidx is not used for the local profiles */
    pidx = profiles->localstack [profiter->localidx];
    profiter->localidx -= 1;
    return pidx;
  }

  pidx = profiter->pidx;

  if (pidx == MKC_PROF_NOT_FOUND) {
    /* the first time, the active profile is one of */
    /* target/compiler, current/compiler or current/general */
    /* if a target type profile, use the target */
    /* otherwise, use the temporary profile */

    if (profiter->ptype == MKC_PROF_TYPE_TARGET) {
      pidx = profiter->origpidx;
      profiter->pidx = pidx;
      return pidx;
    }

    pidx = mkc_profile_find (profiles, MKC_C_PROF_TEMP_NAME, MKC_COMPILER_GENERAL);
    profiter->ptype = MKC_PROF_TYPE_TEMP;
    profiter->pidx = pidx;
    return pidx;
  }

  if (profiter->ptype == MKC_PROF_TYPE_CURRENT &&
      mkc_profile_get_compiler (profiles, profiter->pidx) != MKC_COMPILER_GENERAL) {
    /* if the compiler is not set to general, check the */
    /* profile with the same name, with the compiler set to general */
    /* note that this profile always exists */
    pidx = mkc_profile_find (profiles, profiter->pname, MKC_COMPILER_GENERAL);
    profiter->pidx = pidx;
    return pidx;
  }

  /* check the profile type, and locate the profile */
  /* for the next type in the hierarchy */

  switch (profiter->ptype) {
    case MKC_PROF_TYPE_INVALID: {
      mkc_error_set (profiles->mkcerr, MKC_ERR_INVALID_PROFILE, 0, NULL);
      break;
    }
    case MKC_PROF_TYPE_LOCAL: {
      /* local-type already handled above */
      break;
    }
    case MKC_PROF_TYPE_TARGET: {
      /* temporary profile is next */
      pidx = mkc_profile_find (profiles, MKC_C_PROF_TEMP_NAME, MKC_COMPILER_GENERAL);
      profiter->ptype = MKC_PROF_TYPE_TEMP;
      profiter->pidx = pidx;
      break;
    }
    case MKC_PROF_TYPE_TEMP: {
      /* find the profile using the current profile name */
      pidx = mkc_profile_find (profiles, profiter->pname, profiles->dfltcompiler);

      /* this is the one situation where the next profile may not exist */
      if (pidx == MKC_PROF_NOT_FOUND) {
        pidx = mkc_profile_find (profiles, profiter->pname, MKC_COMPILER_GENERAL);
      }

      profiter->ptype = MKC_PROF_TYPE_CURRENT;
      profiter->pidx = pidx;
      break;
    }
    case MKC_PROF_TYPE_CURRENT: {
      pidx = mkc_profile_find (profiles,
          MKC_C_PROF_INTERNAL_NAME, MKC_COMPILER_GENERAL);
      profiter->ptype = MKC_PROF_TYPE_INTERNAL;
      profiter->pidx = pidx;
      break;
    }
    case MKC_PROF_TYPE_INTERNAL: {
      pidx = MKC_PROF_NOT_FOUND;
      profiter->pidx = pidx;
      break;
    }
  } /* switch on the profile type */

  return pidx;
}

const char *
mkc_profile_curr_disp (mkc_profile_t *profiles, char *buff, size_t sz)
{
  mkc_profidx_t   pidx;
  const char      *nm;
  char            *p;
  mkc_compiler_t compiler;

  pidx = mkc_profile_get_active (profiles);
  nm = mkc_profile_get_name (profiles, pidx);
  compiler = mkc_profile_get_compiler (profiles, pidx);
  p = stpecpy (buff, buff + sz, nm);
  if (compiler != MKC_COMPILER_GENERAL) {
    p = stpecpy (p, buff + sz, "/");
    p = stpecpy (p, buff + sz, mkc_compiler_get_name (compiler));
  }
  return buff;
}

bool
mkc_profile_is_current (mkc_profile_t *profiles, const char *reqnm)
{
  mkc_prof_entry_t  *pentry;
  const char        *currnm;
  mkc_profidx_t     pidx;
  mkc_compiler_t    tcompiler;

  if (profiles == NULL) {
    return false;
  }

  tcompiler = profiles->dfltcompiler;
  if (strcmp (reqnm, MKC_C_PROF_INTERNAL_NAME) == 0) {
    tcompiler = MKC_COMPILER_GENERAL;
  }
  pidx = mkc_profile_find (profiles, reqnm, tcompiler);
  if (pidx != MKC_PROF_NOT_FOUND) {
    pentry = mkc_list_get_by_idx (profiles->list, pidx);
    /* the internal profile is always allowed */
    if (pentry->type == MKC_PROF_TYPE_INTERNAL) {
      return true;
    }
  }

  currnm = mkc_profile_get_name (profiles, profiles->current_idx);

  if (strcmp (currnm, reqnm) == 0) {
    return true;
  }

  return false;
}

/* internal routines */

static void
mkc_profile_entry_free (void *tentry)
{
  mkc_prof_entry_t *pentry = tentry;

  if (pentry == NULL) {
    return;
  }

  datafree (pentry->name);
  if (pentry->varlist != NULL) {
    mkc_varlist_free (pentry->varlist);
  }
}

static int
mkc_profile_compare (void *tentrya, void *tentryb)
{
  mkc_prof_entry_t   *pentrya = tentrya;
  mkc_prof_entry_t   *pentryb = tentryb;
  int                rc = 0;

  if (pentrya == NULL || pentryb == NULL) {
    return 0;
  }

  rc = strcmp (pentrya->name, pentryb->name);
  if (rc == 0) {
    if (pentrya->compiler > pentryb->compiler) {
      rc = 1;
    }
    if (pentrya->compiler < pentryb->compiler) {
      rc = -1;
    }
  }

  return rc;
}

