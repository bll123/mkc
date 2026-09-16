/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "mkc_const.h"
#include "mkc_def.h"
#include "mkc_error.h"
#include "list.h"
#include "var.h"
#include "strutil.h"
#include "value.h"

typedef struct var_t {
  char          * name;
  value_t       value;
  bool          fromcache;
} var_t;

typedef struct varlist_t     {
  list_t        * list;
  mkc_error_t   * mkcerr;
  mkc_log_t     * log;
  bool          debug;
  bool          fromcache;
} varlist_t;

static var_t *var_create (varlist_t     *varlist, const char *vname, value_type_t type);
static mkc_varidx_t var_find (varlist_t     *varlist, const char *name);
static void var_free (void *data);
static int var_compare (void *tvara, void *tvarb);

MKC_NODISCARD
varlist_t     *
varlist_init (mkc_log_t *log, mkc_error_t *mkcerr)
{
  varlist_t      *varlist;

  varlist = malloc (sizeof (varlist_t));
  if (varlist == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  varlist->list = list_init (MKC_LIST_SORTED,
      var_free, var_compare, sizeof (var_t), mkcerr);
  varlist->debug = false;
  varlist->mkcerr = mkcerr;
  varlist->log = log;
  varlist->fromcache = false;

  return varlist;
}

void
varlist_free (varlist_t     *varlist)
{
  if (varlist == NULL) {
    return;
  }

  if (varlist->list != NULL) {
    list_free (varlist->list);
  }

  free (varlist);
}

void
var_set_fromcache (varlist_t     *varlist, bool flag)
{
  if (varlist == NULL) {
    return;
  }

  varlist->fromcache = flag;
}

int
var_set (varlist_t     *varlist, const char *vname, value_t *value)
{
  mkc_err_code_t  rc = MKC_OK;
  var_t       *var;
  mkc_varidx_t    vidx;
  value_t         *tvalue;
  value_t         valuecopy;
  value_type_t    nvtype;

  /* need a copy of the value, as it may get trashed by a realloc */
  memcpy (&valuecopy, value, sizeof (value_t));
  value = &valuecopy;

  vidx = var_find (varlist, vname);
  if (vidx == MKC_VAR_NOTFOUND) {
    var = var_create (varlist, vname, value->vtype);
    if (mkc_error_chk_err (varlist->mkcerr)) {
      return MKC_ERR_FAILURE;
    }
  } else {
    var = list_get_by_idx (varlist->list, vidx);
  }

  tvalue = &var->value;

  nvtype = value->vtype;
  if (value_is_string_type (value)) {
    nvtype = MKC_VT_STRING;
  }

  if (tvalue->vtype != MKC_VT_INVALID) {
    /* free any old stored value */
    value_free (tvalue);
  }
  value_copy (tvalue, value, varlist->mkcerr);
  tvalue->vtype = nvtype;

  /* check to see if a variable from the cache has changed */
  /* only do this if the cache is currently loading */
  if (varlist->fromcache && var->fromcache != varlist->fromcache) {
    /* changing the profile name does not invalidate the cache */
    if (strcmp (vname, MKC_C_PROFILE_NAME) != 0) {
      if (tvalue->vtype == MKC_VT_STRING && nvtype == MKC_VT_STRING) {
        if (strcmp (tvalue->sval, value->sval) != 0) {
          mkc_log (varlist->log, MKC_LOG_VAR, "invalidate cache: %s : %s %s\n",
              vname, tvalue->sval, value->sval);
          rc = MKC_OK_CHANGE;
        }
      }
      if (tvalue->vtype == MKC_VT_INTEGER && nvtype == MKC_VT_INTEGER) {
        if (tvalue->ival != value->ival) {
          mkc_log (varlist->log, MKC_LOG_VAR,
              "invalidate cache: %s : %" PRId32 "%" PRId32 "\n",
              vname, tvalue->ival, value->ival);
          rc = MKC_OK_CHANGE;
        }
      }
      if (tvalue->vtype == MKC_VT_TIMESTAMP && nvtype == MKC_VT_TIMESTAMP) {
        if (tvalue->tmval != value->tmval) {
          mkc_log (varlist->log, MKC_LOG_VAR,
              "invalidate cache: %s : %" PRId64 "%" PRId64 "\n",
              vname, tvalue->tmval, value->tmval);
          rc = MKC_OK_CHANGE;
        }
      }
    }
  }

  return rc;
}

void
var_delete (varlist_t     *varlist, const char *vname)
{
  mkc_varidx_t    vidx;

  vidx = var_find (varlist, vname);
  if (vidx == MKC_VAR_NOTFOUND) {
    return;
  }

  list_delete (varlist->list, vidx);
}

void
var_set_context (varlist_t     *varlist, const char *vname, int vctxt)
{
  listidx_t   vidx;
  var_t       *var;
  value_t     *tvalue;

  if (varlist == NULL) {
    return;
  }
  if (vname == NULL) {
    mkc_error_set (varlist->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  vidx = var_find (varlist, vname);
  if (vidx == MKC_VAR_NOTFOUND) {
    return;
  }

  var = list_get_by_idx (varlist->list, vidx);
  tvalue = &var->value;
  tvalue->vctxt = vctxt;
}

int32_t
var_size (varlist_t     *varlist)
{
  int32_t     sz;

  if (varlist == NULL) {
    return 0;
  }

  sz = list_size (varlist->list);
  return sz;
}

void
var_iter_start (varlist_t     *varlist, mkc_varidx_t *iteridx)
{
  if (varlist == NULL || iteridx == NULL) {
    return;
  }

  list_iter_start (varlist->list, iteridx);
}

mkc_varidx_t
var_iter_next (varlist_t     *varlist, mkc_varidx_t *iteridx)
{
  mkc_varidx_t   idx;

  if (varlist == NULL || iteridx == NULL) {
    return MKC_ITER_FINISH;
  }

  idx = list_iter_next (varlist->list, iteridx);
  return idx;
}

value_t *
var_get_value (varlist_t     *varlist, const char *name)
{
  mkc_varidx_t  vidx = MKC_VAR_NOTFOUND;
  var_t     *var;
  value_t       *value;

  if (varlist == NULL) {
    return NULL;
  }

  if (list_size (varlist->list) == 0) {
    return NULL;
  }

  vidx = var_find (varlist, name);
  if (vidx == MKC_VAR_NOTFOUND) {
    return NULL;
  }

  var = list_get_by_idx (varlist->list, vidx);
  if (var == NULL) {
    return NULL;
  }

  value = &var->value;
  return value;
}

value_t *
var_get_value_by_idx (varlist_t     *varlist, mkc_varidx_t vidx)
{
  var_t     *var;
  value_t       *value;

  if (varlist == NULL) {
    return 0;
  }

  var = list_get_by_idx (varlist->list, vidx);
  if (var == NULL) {
    return NULL;
  }
  value = &var->value;

  return value;
}

const char *
var_get_name (varlist_t     *varlist, mkc_varidx_t vidx)
{
  var_t     *var;
  const char    *nm;

  if (varlist == NULL) {
    return 0;
  }

  var = list_get_by_idx (varlist->list, vidx);
  if (var == NULL) {
    return NULL;
  }
  nm = var->name;

  return nm;
}

bool
var_is_defined (varlist_t     *varlist, const char *vname)
{
  mkc_varidx_t    vidx;
  bool            rc = false;

  if (varlist == NULL) {
    return rc;
  }

  vidx = var_find (varlist, vname);
  if (vidx != MKC_ERR_FAILURE && vidx != MKC_VAR_NOTFOUND) {
    rc = true;
  }

  return rc;
}

bool
var_is_list (varlist_t     *varlist, const char *vname)
{
  mkc_varidx_t    vidx;
  bool            rc = false;

  vidx = var_find (varlist, vname);
  if (vidx != MKC_VAR_NOTFOUND) {
    var_t   *var;
    value_t *value;

    var = list_get_by_idx (varlist->list, vidx);
    if (var == NULL) {
      return rc;
    }
    value = &var->value;
    if (value->vtype == MKC_VT_LIST) {
      rc = true;
    }
  }

  return rc;
}

/* internal routines */

static var_t *
var_create (varlist_t     *varlist,
    const char *name, value_type_t vtype)
{
  var_t     *var;
  var_t     tvar;

  if (name == NULL) {
    mkc_error_set (varlist->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

  tvar.name = strdup (name);
  value_init (&tvar.value);
  tvar.fromcache = varlist->fromcache;

  var = list_set (varlist->list, &tvar);

  return var;
}

static mkc_varidx_t
var_find (varlist_t     *varlist, const char *name)
{
  var_t     tvar;
  mkc_varidx_t  idx;

  if (varlist == NULL) {
    return MKC_ERR_FAILURE;
  }

  tvar.name = (char *) name;
  idx = list_find (varlist->list, &tvar);
  return idx;
}

static void
var_free (void *data)
{
  var_t   *var = data;

  if (var == NULL) {
    return;
  }

  free (var->name);
  value_free (&var->value);
}

static int
var_compare (void *tvara, void *tvarb)
{
  var_t   *vara = tvara;
  var_t   *varb = tvarb;

  if (vara == NULL || varb == NULL) {
    return 0;
  }

  return strcmp (vara->name, varb->name);
}

