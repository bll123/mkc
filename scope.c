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
#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_list.h"   // for the iterator enums
#include "mkc_log.h"
#include "mkc_var.h"
#include "scope.h"
#include "strutil.h"
#include "value.h"

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
  scope_varlist_t othervars;
  mkc_error_t     * mkcerr;
  mkc_log_t       * log;
  mkc_compiler_t  dfltcompiler;
  mkc_compiler_t  currcompiler;
  int             standardsz;
  int             comp_idx;
  bool            fromcache;
} scope_t;

static void scope_free_variables (scope_varlist_t *variables, int skip);
static mkc_varlist_t * scope_push_variables (scope_t *scope, scope_varlist_t *variables, scope_type_t sctype);
static void scope_get_variable_str (scope_t *scope, value_t *value, char *buff, size_t sz);
static void scope_sub_escapes (char *buff, size_t blen);
static int32_t scope_get_variable_integer (scope_t *scope, value_t *value);
static value_t * scope_get_variable_value (scope_t *scope, const char *str);

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
  scope->othervars.variables = NULL;
  scope->othervars.allocsz = 0;
  scope->othervars.sz = 0;
  scope->dfltcompiler = MKC_COMPILER_GENERAL;
  scope->currcompiler = MKC_COMPILER_GENERAL;
  scope->fromcache = false;

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

  scope_push (scope, SCOPE_T_TIMESTAMP);
  scope_push (scope, SCOPE_T_DEPENDENCIES);
  scope_push (scope, SCOPE_T_PATHS);

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
  scope_free_variables (&scope->othervars, -1);
  free (scope);
}

void
scope_push (scope_t *scope, scope_type_t sctype)
{
  if (scope->currcompiler == MKC_COMPILER_GENERAL) {
    scope_push_variables (scope, &scope->variables, sctype);
  } else if (sctype == SCOPE_T_TIMESTAMP ||
      sctype == SCOPE_T_DEPENDENCIES ||
      sctype == SCOPE_T_PATHS) {
    scope_push_variables (scope, &scope->othervars, sctype);
  } else {
    mkc_varlist_t   *vars;
    scope_var_t     *scvar;

    vars = scope_push_variables (scope, &scope->compilervars, sctype);
    scvar = &scope->variables.variables [scope->comp_idx];
    scvar->varlist = vars;
    scvar->compiler = scope->currcompiler;
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
scope_set_curr_compiler (scope_t *scope, mkc_compiler_t compiler)
{
  if (scope == NULL) {
    return;
  }

  scope->currcompiler = compiler;
}

void
scope_set_fromcache (scope_t *scope, bool flag)
{
  if (scope == NULL) {
    return;
  }

  scope->fromcache = flag;
}

/* get */

time_t
scope_get_timestamp (scope_t *scope, const char *vname)
{
  value_t     *value;

  value = scope_get_value (scope, SCOPE_T_IN_SCOPE, vname);
  return scope_value_get_timestamp (scope, value);
}

value_t *
scope_get_value (scope_t *scope, scope_type_t sctype, const char *vname)
{
  scope_var_t   * scvar;
  value_t       * value = NULL;
  mkc_varlist_t * varlist = NULL;

  /* handle the special namespaces */
  if (sctype == SCOPE_T_TIMESTAMP ||
      sctype == SCOPE_T_DEPENDENCIES ||
      sctype == SCOPE_T_PATHS) {
    int     idx = -1;

    for (int i = 0; i < scope->othervars.sz; ++i) {
      if (scope->othervars.variables [i].sctype == sctype) {
        idx = i;
        break;
      }
    }

    if (idx == -1) {
      mkc_error_set (scope->mkcerr, MKC_ERR_FATAL_ERROR, 0, NULL);
      return NULL;
    }

    scvar = &scope->othervars.variables [idx];
    varlist = scvar->varlist;
    value = mkc_var_get_value (varlist, vname);

    return value;
  }

  for (int i = scope->variables.sz - 1; i >= 0; --i) {
    mkc_varlist_t   *varlist;

    scvar = &scope->variables.variables [i];
    if (sctype != SCOPE_T_IN_SCOPE &&
        scvar->sctype != sctype) {
      /* if a particular scope is selected */
      continue;
    }

    varlist = scvar->varlist;
    value = mkc_var_get_value (varlist, vname);
    if (value != NULL) {
      break;
    }

    if (sctype != SCOPE_T_IN_SCOPE &&
        scvar->sctype == sctype) {
      /* if a particular scope is selected */
      break;
    }
  }

  return value;
}

int32_t
scope_value_get_integer (scope_t *scope, value_t *value)
{
  int32_t       ival = 0;

  if (value == NULL) {
    mkc_error_set (scope->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return 0;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
      break;
    }
    case MKC_VT_RANGE:
    case MKC_VT_TIMESTAMP: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_INTEGER: {
      ival = value->ival;
      break;
    }
    case MKC_VT_LIST: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
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
      ival = scope_get_variable_integer (scope, value);
      break;
    }
    case MKC_VT_STRING:
    case MKC_VT_STATIC_STRING:
    case MKC_VT_QUOTED_STRING: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
  }

  mkc_log (scope->log, MKC_LOG_PROCESS, "  scope-get-int: %" PRId32 "\n", ival);
  return ival;
}

time_t
scope_value_get_timestamp (scope_t *scope, value_t *value)
{
  time_t    tmval = 0;

  if (value == NULL) {
    mkc_error_set (scope->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return 0;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_RANGE: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_TIMESTAMP: {
      tmval = value->tmval;
      break;
    }
    case MKC_VT_INTEGER:
    case MKC_VT_LIST: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
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

      tvalue = scope_get_variable_value (scope, value->sval);
      if (tvalue == NULL) {
        mkc_error_set (scope->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
        return 0;
      }
      if (tvalue->vtype == MKC_VT_TIMESTAMP) {
        tmval = tvalue->tmval;
      } else {
        mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      }
      break;
    }
    case MKC_VT_STRING:
    case MKC_VT_STATIC_STRING:
    case MKC_VT_QUOTED_STRING: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
  }

  mkc_log (scope->log, MKC_LOG_PROCESS, "  pv-get-int: %" PRId64 "\n", tmval);
  return tmval;
}

void
scope_value_get_str (scope_t *scope, value_t *value,
    char *buff, size_t sz)
{
  *buff = '\0';

  if (value == NULL) {
    mkc_error_set (scope->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
      break;
    }
    case MKC_VT_RANGE: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
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

      tbuff = scope_substitute (scope, value->sval, SCOPE_SUB_ESCAPE, 0);
      stpecpy (buff, buff + sz, tbuff);
      free (tbuff);
      break;
    }
    case MKC_VT_LIST: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      break;
    }
    case MKC_VT_ENV_VARIABLE: {
      env_get (value->sval, buff, sz);
      break;
    }
    case MKC_VT_VARIABLE: {
      scope_get_variable_str (scope, value, buff, sz);
      break;
    }
  }

  mkc_log (scope->log, MKC_LOG_PROCESS, "  scope-get-str: %s\n", buff);
}

/* get the actual value of a value */
/* this is only an issue for env-variables, quoted strings and lists */
/* the caller is responsible for calling scope_temp_value_free() */
value_t *
scope_value_get_value (scope_t *scope, value_t *value)
{
  value_t   *tvalue;
  value_t   *nvalue;

  /* in many cases the value returned is simply the value passed in */
  nvalue = value;

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
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
        mkc_error_set (scope->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
        return nvalue;
      }

      /* need to get the actual value */
      scope_value_get_str (scope, value, buff, MKC_PATH_MAX);

      tvalue = malloc (sizeof (value_t));
      if (tvalue == NULL) {
        mkc_error_set (scope->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
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
      nvalue = scope_get_variable_value (scope, value->sval);
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

      nlist = mkc_list_init (MKC_LIST_UNSORTED, scope_temp_value_free, NULL, scope->mkcerr);
      mkc_list_iter_start (value->list, &iteridx);
      while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
        value_t   *lvalue;
        value_t   *tmpvalue;
        mkc_listidx_t loc = MKC_LIST_NOTFOUND;

        if (mkc_error_chk_err (scope->mkcerr)) {
          break;
        }

        lvalue = mkc_list_get_by_idx (value->list, lidx);
        tmpvalue = scope_value_get_value (scope, lvalue);
        mkc_list_set (nlist, tmpvalue, sizeof (value_t), &loc);
      }

      tvalue = malloc (sizeof (value_t));
      if (tvalue == NULL) {
        mkc_error_set (scope->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
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
scope_value_get_list_value (scope_t *scope, value_t *value)
{
  value_t    *rvalue = NULL;

  if (value == NULL) {
    mkc_error_set (scope->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
      break;
    }
    case MKC_VT_ENV_VARIABLE:
    case MKC_VT_INTEGER:
    case MKC_VT_QUOTED_STRING:
    case MKC_VT_STATIC_STRING:
    case MKC_VT_STRING:
    case MKC_VT_TIMESTAMP: {
      mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
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
      value = scope_get_variable_value (scope, value->sval);
      if (value->vtype == MKC_VT_LIST) {
        rvalue = value;
      } else {
        mkc_error_set (scope->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
      }
      break;
    }
  }

  return rvalue;
}

/* will create the value/list if it does not exist */
/* directly append the string to a value containing a list */
/* this is called using a known list value, so there are no */
/* verification checks */
int
scope_append_str_list (scope_t *scope, scope_type_t sctype,
    const char *vname, const char *data, value_ctxt_t vctxt)
{
  value_t   *listval;
  mkc_list_t    *list;
  mkc_listidx_t loc;
  value_t   tvalue;


  listval = scope_get_value (scope, sctype, vname);
  if (listval == NULL) {
    list = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, scope->mkcerr);
    scope_set_list (scope, sctype, vname, list, vctxt);
    listval = scope_get_value (scope, sctype, vname);
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

/* set */

void
scope_set_context (scope_t *scope, const char *vname,
    value_ctxt_t vctxt)
{
  value_t   *value;

  if (scope == NULL || vname == NULL) {
    return;
  }

  value = scope_get_value (scope, SCOPE_T_IN_SCOPE, vname);
  if (value != NULL) {
    value->vctxt = vctxt;
  }
}

int
scope_set (scope_t *scope, scope_type_t sctype, const char *vname,
    value_t *value, value_ctxt_t vctxt)
{
  mkc_varlist_t   *varlist = NULL;
  int             rc = MKC_ERR_FAILURE;
  int             idx = -1;

  if (scope == NULL || vname == NULL || value == NULL) {
    return rc;
  }

  if (sctype == SCOPE_T_TIMESTAMP ||
      sctype == SCOPE_T_DEPENDENCIES ||
      sctype == SCOPE_T_PATHS) {
    for (int i = 0; i < scope->othervars.sz; ++i) {
      if (scope->othervars.variables [i].sctype == sctype) {
        idx = i;
        break;
      }
    }

    if (idx == -1) {
      return rc;
    }

    varlist = scope->othervars.variables [idx].varlist;
  } else {
    /* if the inlocal flag is set, set the variable in the local variable list */
    /* otherwise, set the variable in the currently active profile */
  }

  value->vctxt = vctxt;
  mkc_var_set_fromcache (varlist, scope->fromcache);
  rc = mkc_var_set (varlist, vname, value);

  return MKC_OK;
}

int
scope_set_integer (scope_t *scope, scope_type_t sctype,
    const char *vname, int32_t ival, value_ctxt_t vctxt)
{
  int         rc = MKC_ERR_FAILURE;
  value_t value;

  value_init (&value);
  value.ival = ival;
  value.vtype = MKC_VT_INTEGER;

  rc = scope_set (scope, sctype, vname, &value, vctxt);
  return rc;
}

int
scope_set_timestamp (scope_t *scope, scope_type_t sctype,
    const char *vname, time_t tmval, value_ctxt_t vctxt)
{
  int         rc = MKC_ERR_FAILURE;
  value_t value;

  value_init (&value);
  value.tmval = tmval;
  value.vtype = MKC_VT_TIMESTAMP;

  rc = scope_set (scope, sctype, vname, &value, vctxt);
  return rc;
}

int
scope_set_str (scope_t *scope, scope_type_t sctype,
    const char *vname, const char *str, value_ctxt_t vctxt)
{
  int         rc = MKC_ERR_FAILURE;
  value_t value;

  value_init (&value);
  value.sval = (char *) str;
  value.vtype = MKC_VT_STRING;

  rc = scope_set (scope, sctype, vname, &value, vctxt);
  return rc;
}

int
scope_set_list (scope_t *scope, scope_type_t sctype,
    const char *vname, mkc_list_t *list, value_ctxt_t vctxt)
{
  int         rc = MKC_ERR_FAILURE;
  value_t value;

  value_init (&value);
  value.list = list;
  value.vtype = MKC_VT_LIST;

  rc = scope_set (scope, sctype, vname, &value, vctxt);
  return rc;
}

int
scope_set_list_from_str (scope_t *scope,
    const char *vname, char *str, value_ctxt_t vctxt)
{
  int           rc = MKC_ERR_FAILURE;
  char          *p;
  char          *tokstr;

  p = str_token (str, " ", &tokstr);
  while (p != NULL) {
    if (mkc_error_chk_err (scope->mkcerr)) {
      return MKC_ERR_FAILURE;
    }

    str_trim (p, 0);
    scope_append_str_list (scope, SCOPE_T_IN_SCOPE, vname, p, vctxt);
    p = str_token (NULL, " ", &tokstr);
  }

  return rc;
}

bool
scope_is_defined (scope_t *scope, const char *vname)
{
  value_t     *value;

  if (scope == NULL) {
    return false;
  }

  value = scope_get_value (scope, SCOPE_T_IN_SCOPE, vname);
  if (value == NULL) {
    return false;
  }
  return true;
}

bool
scope_var_is_list (scope_t *scope, const char *vname)
{
  value_t     *value;
  bool        rc = false;

  if (scope == NULL) {
    return rc;
  }

  value = scope_get_value (scope, SCOPE_T_IN_SCOPE, vname);
  if (value == NULL) {
    return rc;
  }

  if (value->vtype == MKC_VT_LIST || value->vtype == MKC_VT_RANGE) {
    rc = true;
  }
  return rc;
}

void
scope_temp_value_free (void *tvalue)
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
/* by the caller by calling scope_get_value () */
char *
scope_substitute (scope_t *scope, const char *data,
    scope_escape_t subescapeflag, int depth)
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

  if (scope == NULL) {
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
    if (subescapeflag == SCOPE_SUB_ESCAPE) {
      scope_sub_escapes (buff, blen);
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
        mkc_error_set (scope->mkcerr, MKC_ERR_UNBALANCED_BRACES, 0, NULL);
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
      tstr = scope_substitute (scope, substr, SCOPE_NO_ESCAPE, depth + 1);
      free (substr);
//fprintf (stderr, "%*ststr: '%s'\n", depth * 2, "", tstr);

      if (isenv) {
        env_get (tstr, ebuff, sizeof (ebuff));
        tval = ebuff;
      } else {
        value_t   *value;

        value = scope_get_value (scope, SCOPE_T_IN_SCOPE, tstr);
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
  if (subescapeflag == SCOPE_SUB_ESCAPE) {
    scope_sub_escapes (buff, blen);
  }
  return buff;
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

static void
scope_get_variable_str (scope_t *scope, value_t *value,
    char *buff, size_t sz)
{
  value_t     *tvalue;

  if (scope == NULL) {
    return;
  }

  *buff = '\0';

  tvalue = scope_get_variable_value (scope, value->sval);
  if (tvalue == NULL) {
    mkc_error_set (scope->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
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

    mkc_log (scope->log, MKC_LOG_PROCESS, "  scope-get-var-str: %s\n",
        value_to_str (tvalue, dbuff, sizeof (dbuff)));
  }
}

static void
scope_sub_escapes (char *buff, size_t blen)
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
scope_get_variable_integer (scope_t *scope, value_t *value)
{
  int32_t     ival = 0;
  value_t     *tvalue;

  tvalue = scope_get_variable_value (scope, value->sval);
  if (tvalue == NULL) {
    mkc_error_set (scope->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, NULL);
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
scope_get_variable_value (scope_t *scope, const char *str)
{
  char        *tstr;
  value_t *value;

  tstr = scope_substitute (scope, str, SCOPE_NO_ESCAPE, 0);
  value = scope_get_value (scope, SCOPE_T_IN_SCOPE, tstr);
  free (tstr);
  return value;
}

