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
#include "mkc_error.h"
#include "mkc_list.h"   // for the iterator enums
#include "mkc_log.h"
#include "mkc_var.h"
#include "scope.h"

typedef struct scope_var_t {
  mkc_varlist_t   * varlist;
  scope_type_t    sctype;
  mkc_compiler_t  compiler;
} scope_var_t;

typedef struct scope_varlist_t {
  scope_var_t     * variables;
  int             allocsz;
  int             sz;
} scope_varlist_t;

typedef struct scope_t {
  scope_varlist_t variables;
  scope_varlist_t compilervars;
  mkc_error_t     * mkcerr;
  mkc_log_t       * log;
  mkc_compiler_t  dfltcompiler;
  mkc_compiler_t  currcompiler;
  int             standardsz;
  int             comp_idx;
} scope_t;

static void scope_free_variables (scope_varlist_t *variables, int skip);
static mkc_varlist_t * scope_push_variables (scope_t *scope, scope_varlist_t *variables, scope_type_t sctype);

scope_t *
scope_init (mkc_log_t *log, mkc_error_t *mkcerr)
{
  scope_t   *scope;

  scope = malloc (sizeof (scope_t));
  if (scope == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  scope->mkcerr = mkcerr;
  scope->log = log;
  scope->variables.variables = NULL;
  scope->variables.allocsz = 0;
  scope->variables.sz = 0;
  scope->compilervars.variables = NULL;
  scope->compilervars.allocsz = 0;
  scope->compilervars.sz = 0;
  scope->dfltcompiler = MKC_COMPILER_GENERAL;
  scope->currcompiler = MKC_COMPILER_GENERAL;

  /* create the standard set of scopes */
  /* when searching for a variable, the scopes will be traversed */
  /* in reverse order */
  scope_push (scope, SCOPE_T_INTERNAL);
  scope_push (scope, SCOPE_T_CURR_PROF);
  scope_push (scope, SCOPE_T_CURR_PROF_COMPILER);
  scope->comp_idx = scope->variables.sz - 1;
  /* there will always be a local scope at the top level */
  scope_push (scope, SCOPE_T_LOCAL);
  scope->standardsz = scope->variables.sz;

  return scope;
}

void
scope_free (scope_t *scope)
{
  if (scope == NULL) {
    return;
  }
  scope_free_variables (&scope->variables, SCOPE_T_CURR_PROF_COMPILER);
  scope_free_variables (&scope->compilervars, -1);
  free (scope);
}

void
scope_push (scope_t *scope, scope_type_t sctype)
{
  if (scope->currcompiler == MKC_COMPILER_GENERAL) {
    scope_push_variables (scope, &scope->variables, sctype);
  } else {
    mkc_varlist_t   *vars;
    scope_var_t     *scvar;

    vars = scope_push_variables (scope, &scope->compilervars, sctype);
    scvar = &scope->variables.variables [scope->comp_idx];
    scvar->varlist = vars;
  }
}

void
scope_pop (scope_t *scope)
{
  scope_varlist_t   *scvarlist;
  scope_var_t       *scvar;

  if (scope == NULL) {
    return;
  }

  scvarlist = &scope->variables;
  if (scvarlist->sz <= 0) {
    mkc_error_set (scope->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, "scope");
    return;
  }

  /* the standard scopes should never get popped off of the stack */
  if (scvarlist->sz == scope->standardsz) {
    mkc_error_set (scope->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, "scope-b");
    return;
  }

  scvarlist->sz -= 1;
  scvar = &scvarlist->variables [scvarlist->sz];

  mkc_varlist_free (scvar->varlist);
  scvar->varlist = NULL;
  scvar->sctype = SCOPE_T_NOT_IN_USE;
}

void
scope_set_compiler (scope_t *scope, mkc_compiler_t compiler)
{
  if (scope == NULL) {
    return;
  }

  scope->currcompiler = compiler;
}

value_t *
scope_get_var (scope_t *scope, const char *vname)
{
  scope_var_t   *scvar;
  value_t   *value = NULL;

  for (int i = scope->variables.sz - 1; i >= 0; --i) {
    mkc_varlist_t   *varlist;

    scvar = &scope->variables.variables [i];
    varlist = scvar->varlist;
    value = mkc_var_get_value (varlist, vname);
    if (value != NULL) {
      break;
    }
  }

  return value;
}

/* internal routines */

static void
scope_free_variables (scope_varlist_t *variables, int skip)
{
  if (variables != NULL) {
    for (int i = 0; i < variables->sz; ++i) {
      scope_var_t   *scvar;

      if (skip >= 0 && i == skip) {
        /* SCOPE_T_CURR_PROF_COMPILER does not have its own varlist */
        continue;
      }

      scvar = &variables->variables [i];
      mkc_varlist_free (scvar->varlist);
    }
    free (variables);
  }
}

static mkc_varlist_t *
scope_push_variables (scope_t *scope, scope_varlist_t *variables,
    scope_type_t sctype)
{
  scope_var_t   *scvar;

  if (variables->sz >= variables->allocsz) {
    variables->allocsz += 10;
    variables->variables = realloc (variables->variables,
        sizeof (scope_var_t) * variables->allocsz);
    for (int i = variables->sz; i < variables->allocsz; ++i) {
      variables->variables [i].varlist = NULL;
      variables->variables [i].sctype = SCOPE_T_NOT_IN_USE;
    }
  }

  scvar = &variables->variables [variables->sz];
  if (&scope->variables != variables ||
      sctype != SCOPE_T_CURR_PROF_COMPILER) {
    /* the curr-prof-compiler type in the main variables list does */
    /* not own its own varlist */
    scvar->varlist = mkc_varlist_init (scope->log, scope->mkcerr);
  }
  scvar->sctype = sctype;
  variables->sz += 1;

  return scvar->varlist;
}
