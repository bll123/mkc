/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "mkc_error.h"
#include "mkc_list.h"   // for the iterator enums
#include "mkc_log.h"
#include "mkc_var.h"
#include "scope.h"

typedef struct scope_var_t {
  mkc_varlist_t * varlist;
  scope_type_t  sctype;
} scope_var_t;

typedef struct scope_t {
  scope_var_t   * variables;
  mkc_error_t   * mkcerr;
  mkc_log_t     * log;
  int           allocsz;
  int           sz;
  int           standardsz;
} scope_t;

scope_t *
scope_init (mkc_log_t *log, mkc_error_t *mkcerr)
{
  scope_t   *scope;

  scope = malloc (sizeof (scope_t));
  if (scope == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  scope->variables = NULL;
  scope->mkcerr = mkcerr;
  scope->log = log;
  scope->allocsz = 0;
  scope->sz = 0;

  /* create the standard set of scopes */
  /* when searching for a variable, the scopes will be traversed */
  /* in reverse order */
  scope_push (scope, SCOPE_T_INTERNAL);
  scope_push (scope, SCOPE_T_CURR_PROF);
  scope_push (scope, SCOPE_T_CURR_PROF_COMPILER);
  /* there will always be a local scope at the top level */
  scope_push (scope, SCOPE_T_LOCAL);
  scope->standardsz = scope->sz;

  return scope;
}

void
scope_free (scope_t *scope)
{
  if (scope == NULL) {
    return;
  }
  if (scope->variables != NULL) {
    for (int i = 0; i < scope->sz; ++i) {
      scope_var_t   *scvar;

      scvar = &scope->variables [i];
      mkc_varlist_free (scvar->varlist);
    }
    free (scope->variables);
  }
  free (scope);
}

void
scope_push (scope_t *scope, scope_type_t sctype)
{
  scope_var_t   *scvar;

  if (scope->sz >= scope->allocsz) {
    scope->allocsz += 10;
    scope->variables = realloc (scope->variables, sizeof (scope_var_t) * scope->allocsz);
    for (int i = scope->sz; i < scope->allocsz; ++i) {
      scope->variables [i].varlist = NULL;
      scope->variables [i].sctype = SCOPE_T_NOT_IN_USE;
    }
  }

  scvar = &scope->variables [scope->sz];
  scvar->varlist = mkc_varlist_init (scope->log, scope->mkcerr);
  scvar->sctype = sctype;
  scope->sz += 1;
}

void
scope_pop (scope_t *scope)
{
  scope_var_t   *scvar;

  if (scope == NULL) {
    return;
  }

  if (scope->sz <= 0) {
    mkc_error_set (scope->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, "scope");
    return;
  }

  /* the standard scopes should never get popped off of the stack */
  if (scope->sz == scope->standardsz) {
    mkc_error_set (scope->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, "scope-b");
    return;
  }

  scope->sz -= 1;
  scvar = &scope->variables [scope->sz];

  mkc_varlist_free (scvar->varlist);
  scvar->varlist = NULL;
  scvar->sctype = SCOPE_T_NOT_IN_USE;
}

void
scope_iter_start (scope_t *scope, int *iteridx)
{
  *iteridx = MKC_ITER_FINISH;
}

/* the scope iterator always starts at the last entry */
/* and goes to the first entry */
mkc_varlist_t *
scope_iter_next (scope_t *scope, int *iteridx)
{
  scope_var_t   *scvar;

  if (*iteridx == MKC_ITER_FINISH) {
    *iteridx = scope->sz - 1;
  } else {
    *iteridx -= 1;
  }
  if (*iteridx < 0) {
    *iteridx = MKC_ITER_FINISH;
  }

  if (*iteridx == MKC_ITER_FINISH) {
    return NULL;
  }

  scvar = &scope->variables [*iteridx];
  return scvar->varlist;
}

mkc_varlist_t *
scope_get (scope_t *scope, scope_type_t sctype)
{
  scope_var_t   *scvar;
  int           idx = MKC_ITER_FINISH;

  /* as always, search from the last entry to the first */
  for (int i = scope->sz - 1; i >= 0; --i) {
    scvar = &scope->variables [i];
    if (scvar->sctype == sctype) {
      idx = i;
      break;
    }
  }

  if (idx == MKC_ITER_FINISH) {
    return NULL;
  }

  scvar = &scope->variables [idx];
  return scvar->varlist;
}
