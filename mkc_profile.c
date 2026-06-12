/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mkc_def.h"
#include "mkc_list.h"
#include "mkc_profile.h"
#include "mkc_string.h"
#include "mkc_var.h"

enum {
  MKC_PROF_STACK_MAX = 20,
};

static const char * const compnames [MKC_PROF_COMPILER_MAX] = {
  [MKC_PROF_COMPILER_GENERAL] = "general",
  [MKC_PROF_COMPILER_C] = "c",
  [MKC_PROF_COMPILER_CXX] = "cxx",
  [MKC_PROF_COMPILER_OBJC] = "objc",
};

typedef struct mkc_prof_entry_t {
  mkc_varlist_t     * varlist;
  char              * name;
  mkc_prof_comp_t   compiler;
  mkc_prof_type_t   type;
  mkc_profidx_t     pidx;
} mkc_prof_entry_t;

typedef struct mkc_profile_t {
  mkc_list_t        * list;
  mkc_error_t       * mkcerr;
  mkc_log_t         * log;
  char              * dfltprof;
  mkc_profidx_t     localstack [MKC_PROF_STACK_MAX];
  mkc_profidx_t     stack [MKC_PROF_STACK_MAX];
  mkc_profidx_t     active_idx;
  mkc_profidx_t     user_idx;
  int               stacksz;
  int               localidx;
  int               localstacksz;
} mkc_profile_t;

const char * const MKC_PROF_GLOBAL_NAME = "global";
const char * const MKC_PROF_INTERNAL_NAME = "internal";
const char * const MKC_PROF_RELEASE_NAME = "release";

static void mkc_profile_entry_free (void *pentry);
static int mkc_profile_compare (void *tentrya, void *tentryb);
static mkc_prof_comp_t mkc_profile_comp_id (mkc_profile_t *profiles, const char *compiler);
int mkc_profile_create_id (mkc_profile_t *profiles, const char *pname, mkc_prof_comp_t compiler, mkc_prof_type_t type);

mkc_profile_t *
mkc_profile_init (mkc_log_t *log, mkc_error_t *mkcerr, const char *dfltprof)
{
  mkc_profile_t   *profiles;
  mkc_profidx_t   pidx;

  profiles = malloc (sizeof (mkc_profile_t));
  if (profiles == NULL) {
    *mkcerr = MKC_ERR_OUT_OF_MEMORY;
    return NULL;
  }

  profiles->stacksz = 0;
  profiles->localidx = 0;
  profiles->localstacksz = 0;
  profiles->mkcerr = mkcerr;
  profiles->log = log;
  profiles->dfltprof = strdup (dfltprof);
  if (profiles->dfltprof == NULL) {
    *mkcerr = MKC_ERR_OUT_OF_MEMORY;
    mkc_profile_free (profiles);
    return NULL;
  }

  profiles->list = mkc_list_init (MKC_LIST_SORTED,
      mkc_profile_entry_free, mkc_profile_compare, mkcerr);
  if (profiles->list == NULL) {
    *mkcerr = MKC_ERR_OUT_OF_MEMORY;
    mkc_profile_free (profiles);
    return NULL;
  }

  mkc_profile_create_id (profiles, MKC_PROF_INTERNAL_NAME,
      MKC_PROF_COMPILER_GENERAL, MKC_PROF_TYPE_INTERNAL);
  for (mkc_prof_comp_t i = 0; i < MKC_PROF_COMPILER_MAX; ++i) {
    if (*(profiles->mkcerr) != MKC_OK) {
      break;
    }
    mkc_profile_create_id (profiles, MKC_PROF_GLOBAL_NAME,
        i, MKC_PROF_TYPE_GLOBAL);
  }
  pidx = mkc_profile_create_id (profiles, MKC_PROF_RELEASE_NAME,
      MKC_PROF_COMPILER_GENERAL, MKC_PROF_TYPE_USER);

  pidx = mkc_profile_find_id (profiles, dfltprof, MKC_PROF_COMPILER_GENERAL);
  if (mkc_profile_get_type (profiles, pidx) != MKC_PROF_TYPE_USER) {
    *mkcerr = MKC_ERR_INVALID_PROFILE;
    mkc_profile_free (profiles);
    return NULL;
  }
  if (pidx == MKC_PROF_NOT_FOUND) {
    pidx = mkc_profile_create_id (profiles, dfltprof,
          MKC_PROF_COMPILER_GENERAL, MKC_PROF_TYPE_USER);
  }

  profiles->active_idx = pidx;
  profiles->user_idx = pidx;

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
mkc_profile_create (mkc_profile_t *profiles, const char *pname,
    const char *comptxt, mkc_prof_type_t type)
{
  mkc_prof_comp_t   compiler;
  mkc_profidx_t     pidx;

  if (profiles == NULL) {
    return MKC_ERR_FAILURE;
  }

  compiler = mkc_profile_comp_id (profiles, comptxt);
  pidx = mkc_profile_create_id (profiles, pname, compiler, type);

  return pidx;
}

int
mkc_profile_local_create (mkc_profile_t *profiles)
{
  mkc_prof_entry_t  *pentry;
  mkc_prof_entry_t  tentry;
  mkc_profidx_t     loc;
  char              tbuff [40];

  if (profiles == NULL) {
    return MKC_ERR_FAILURE;
  }

  if (profiles->stacksz >= MKC_PROF_STACK_MAX) {
    *(profiles->mkcerr) = MKC_ERR_OUT_OF_RANGE;
    return MKC_ERR_FAILURE;
  }

  snprintf (tbuff, sizeof (tbuff), "local_%d", profiles->localstacksz);

  tentry.name = strdup (tbuff);
  if (tentry.name == NULL) {
    *(profiles->mkcerr) = MKC_ERR_OUT_OF_MEMORY;
    return MKC_ERR_FAILURE;
  }
  tentry.compiler = MKC_PROF_COMPILER_GENERAL;
  tentry.type = MKC_PROF_TYPE_LOCAL;
  tentry.varlist = mkc_varlist_init (profiles->log, profiles->mkcerr);
  if (tentry.varlist == NULL) {
    return MKC_ERR_FAILURE;
  }

  pentry = mkc_list_append (profiles->list, &tentry,
      sizeof (mkc_prof_entry_t), &loc);
  if (pentry == NULL) {
    *(profiles->mkcerr) = MKC_ERR_OUT_OF_MEMORY;
    return MKC_ERR_FAILURE;
  }

  mkc_log (profiles->log, MKC_LOG_PROFILE, "profile: create-local: %s (%d)\n", tbuff, loc);
  profiles->localstack [profiles->localstacksz] = loc;
  profiles->localstacksz += 1;
  profiles->localidx = 0;

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
  profiles->localidx = 0;
  stackidx = profiles->localstacksz;
  pentry = mkc_list_get_by_idx (profiles->list, profiles->localstack [stackidx]);

  mkc_log (profiles->log, MKC_LOG_PROFILE, "profile: pop-local: %s (%d)\n", pentry->name, pentry->pidx);

  mkc_profile_entry_free (pentry);
}

mkc_profidx_t
mkc_profile_find (mkc_profile_t *profiles, const char *pname,
    const char *comptxt)
{
  mkc_prof_comp_t   compiler;
  mkc_profidx_t     pidx;

  if (profiles == NULL) {
    return MKC_ERR_FAILURE;
  }

  compiler = mkc_profile_comp_id (profiles, comptxt);

  pidx = mkc_profile_find_id (profiles, pname, compiler);
  return pidx;
}

mkc_profidx_t
mkc_profile_find_id (mkc_profile_t *profiles, const char *pname,
    mkc_prof_comp_t compiler)
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

  pentry = mkc_list_get_by_idx (profiles->list, pidx);
  if (pentry == NULL) {
    return NULL;
  }

  return pentry->name;
}

mkc_prof_comp_t
mkc_profile_get_compiler (mkc_profile_t *profiles, mkc_profidx_t pidx)
{
  mkc_prof_entry_t   *pentry;

  if (profiles == NULL) {
    return MKC_PROF_COMPILER_GENERAL;
  }

  pentry = mkc_list_get_by_idx (profiles->list, pidx);
  if (pentry == NULL) {
    return MKC_PROF_COMPILER_GENERAL;
  }

  return pentry->compiler;
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
    *(profiles->mkcerr) = MKC_ERR_OUT_OF_RANGE;
    return MKC_ERR_FAILURE;
  }

  profiles->stack [profiles->stacksz] = profiles->active_idx;
  profiles->stacksz += 1;

  mkc_log (profiles->log, MKC_LOG_PROFILE, "profile: push (%d)\n", profiles->active_idx);

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
    *(profiles->mkcerr) = MKC_ERR_OUT_OF_RANGE;
    return MKC_ERR_FAILURE;
  }

  profiles->stacksz -= 1;
  profiles->active_idx = profiles->stack [profiles->stacksz];

  mkc_log (profiles->log, MKC_LOG_PROFILE, "profile: pop (%d)\n", profiles->active_idx);

  pentry = mkc_list_get_by_idx (profiles->list, profiles->active_idx);
  if (pentry->type == MKC_PROF_TYPE_USER) {
    mkc_log (profiles->log, MKC_LOG_PROFILE, "profile: user: %s %s (%d)\n", pentry->name,
        compnames [pentry->compiler], profiles->active_idx);
    profiles->user_idx = profiles->active_idx;
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

  if (pidx < 0 || pidx >= mkc_list_size (profiles->list)) {
    *(profiles->mkcerr) = MKC_ERR_OUT_OF_RANGE;
    return;
  }

  profiles->active_idx = pidx;
  pentry = mkc_list_get_by_idx (profiles->list, pidx);
  if (pentry->type == MKC_PROF_TYPE_USER) {
    mkc_log (profiles->log, MKC_LOG_PROFILE, "profile: user: %s %s (%d)\n", pentry->name, compnames [pentry->compiler], pidx);
    profiles->user_idx = pidx;
  }

  mkc_log (profiles->log, MKC_LOG_PROFILE, "profile: active: %s %s (%d)\n", pentry->name, compnames [pentry->compiler], pidx);
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
mkc_profile_local_reset (mkc_profile_t *profiles)
{
  if (profiles == NULL) {
    return;
  }

  profiles->localidx = 0;
}

/* returns the next profile in the hierarchy (that exists) */
/*    local variable stack */
/*    target / compiler */
/*    target / any */
/*    user / compiler */
/*    user / any */
/*    global / compiler */
/*    global / any */
/*    internal / any */
int
mkc_profile_next (mkc_profile_t *profiles, mkc_prof_comp_t origcompiler)
{
  mkc_prof_entry_t  *pentry;
  mkc_profidx_t     pidx;
  mkc_prof_comp_t   compiler;
  mkc_prof_type_t   type;

  if (profiles == NULL) {
    return MKC_ERR_FAILURE;
  }

  pidx = profiles->active_idx;
  pentry = mkc_list_get_by_idx (profiles->list, pidx);
  compiler = pentry->compiler;
  type = pentry->type;

  if (profiles->localidx < profiles->localstacksz) {
    pidx = profiles->localstack [profiles->localidx];
    profiles->localidx += 1;
    return pidx;
  }

  pidx = MKC_PROF_NOT_FOUND;
  while (pidx == MKC_PROF_NOT_FOUND) {
    if (*(profiles->mkcerr) != MKC_OK) {
      break;
    }

    if (compiler != MKC_PROF_COMPILER_GENERAL) {
      /* if the compiler is not set to general, check the */
      /* profile with the same name, with compiler-general set */
      pidx = mkc_profile_find_id (profiles, pentry->name,
          MKC_PROF_COMPILER_GENERAL);
    } else {
      /* reset the compiler back to the original */
      compiler = origcompiler;

      /* set the type for the next iteration through the while loop */

      switch (type) {
        case MKC_PROF_TYPE_INVALID: {
          *(profiles->mkcerr) = MKC_ERR_INVALID_PROFILE;
          break;
        }
        case MKC_PROF_TYPE_LOCAL: {
          break;
        }
        case MKC_PROF_TYPE_TARGET: {
          const char  *userprof = "ng";

          userprof = mkc_profile_get_name (profiles, profiles->user_idx);
          pidx = mkc_profile_find_id (profiles, userprof, compiler);
          type = MKC_PROF_TYPE_USER;
          break;
        }
        case MKC_PROF_TYPE_USER: {
          pidx = mkc_profile_find_id (profiles,
              MKC_PROF_GLOBAL_NAME, compiler);
          type = MKC_PROF_TYPE_GLOBAL;
          break;
        }
        case MKC_PROF_TYPE_GLOBAL: {
          pidx = mkc_profile_find_id (profiles,
              MKC_PROF_INTERNAL_NAME, MKC_PROF_COMPILER_GENERAL);
          type = MKC_PROF_TYPE_INTERNAL;
          break;
        }
        case MKC_PROF_TYPE_INTERNAL: {
          break;
        }
      } /* switch on the profile type */

      if (type == MKC_PROF_TYPE_INTERNAL) {
        break;
      }
    } /* if the compiler is set to general */

    if (pidx >= 0) {
      pentry = mkc_list_get_by_idx (profiles->list, pidx);
    }
  }

  return pidx;
}

const char *
mkc_profile_curr_disp (mkc_profile_t *profiles, char *buff, size_t sz)
{
  mkc_profidx_t   pidx;
  const char      *nm;
  char            *p;
  mkc_prof_comp_t compiler;

  pidx = mkc_profile_get_active (profiles);
  nm = mkc_profile_get_name (profiles, pidx);
  compiler = mkc_profile_get_compiler (profiles, pidx);
  p = stpecpy (buff, buff + sz, nm);
  if (compiler != MKC_PROF_COMPILER_GENERAL) {
    p = stpecpy (p, buff + sz, "/");
    p = stpecpy (p, buff + sz, compnames [compiler]);
  }
  return buff;
}

bool
mkc_profile_is_current (mkc_profile_t *profiles, const char *reqnm)
{
  mkc_prof_entry_t  *pentry;
  const char        *currnm;
  mkc_profidx_t     pidx;

  if (profiles == NULL) {
    return false;
  }

  pidx = mkc_profile_find_id (profiles, reqnm, MKC_PROF_COMPILER_GENERAL);
  if (pidx != MKC_PROF_NOT_FOUND) {
    pentry = mkc_list_get_by_idx (profiles->list, pidx);
    /* internal and global profiles are always allowed */
    if (pentry->type == MKC_PROF_TYPE_INTERNAL ||
        pentry->type == MKC_PROF_TYPE_GLOBAL) {
      return true;
    }
  }

  currnm = mkc_profile_get_name (profiles, profiles->user_idx);

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

  if (pentry->name != NULL) {
    free (pentry->name);
  }

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

static mkc_prof_comp_t
mkc_profile_comp_id (mkc_profile_t *profiles, const char *compiler)
{
  mkc_prof_comp_t   cid = MKC_PROF_COMPILER_GENERAL;
  bool              found = false;

  if (profiles == NULL || compiler == NULL) {
    return cid;
  }

  for (mkc_prof_comp_t i = MKC_PROF_COMPILER_GENERAL;
      i < MKC_PROF_COMPILER_MAX; ++i) {
    if (strcmp (compiler, compnames [i]) == 0) {
      cid = i;
      found = true;
      break;
    }
  }

  /* check for other variants */
  if (! found) {
    /* the default is MKC_PROF_COMPILER_GENERAL */
    if (strcmp (compiler, "C") == 0) {
      cid = MKC_PROF_COMPILER_C;
    }
    if (strcmp (compiler, "C++") == 0 ||
        strcmp (compiler, "cpp") == 0 ||
        strcmp (compiler, "cxx") == 0) {
      cid = MKC_PROF_COMPILER_CXX;
    }
    if (strcmp (compiler, "OBJC") == 0) {
      cid = MKC_PROF_COMPILER_OBJC;
    }
  }

  return cid;
}

int
mkc_profile_create_id (mkc_profile_t *profiles, const char *pname,
    mkc_prof_comp_t compiler, mkc_prof_type_t type)
{
  mkc_prof_entry_t    *pentry;
  mkc_prof_entry_t    tentry;
  mkc_profidx_t       loc = MKC_LIST_NOTFOUND;

  if (profiles == NULL) {
    return MKC_ERR_FAILURE;
  }

  tentry.name = strdup (pname);
  if (tentry.name == NULL) {
    *(profiles->mkcerr) = MKC_ERR_OUT_OF_MEMORY;
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

  mkc_log (profiles->log, MKC_LOG_PROFILE, "profile: create: %s %s (%d)\n", pname, compnames [compiler], loc);

  return loc;
}

