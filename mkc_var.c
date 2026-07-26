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
#include "mkc_list.h"
#include "mkc_var.h"
#include "strutil.h"
#include "value.h"

typedef struct mkc_var_t {
  char          * name;
  value_t   value;
  bool          fromcache;
} mkc_var_t;

typedef struct mkc_varlist_t {
  mkc_list_t    * list;
  mkc_error_t   * mkcerr;
  mkc_log_t     * log;
  bool          debug;
  bool          fromcache;
} mkc_varlist_t;

static mkc_var_t *mkc_var_create (mkc_varlist_t *varlist, const char *vname, value_type_t type, mkc_listidx_t *loc);
static mkc_varidx_t mkc_var_find (mkc_varlist_t *varlist, const char *name, mkc_listidx_t *loc);
static void mkc_var_free (void *data);
static int mkc_var_compare (void *tvara, void *tvarb);
static mkc_list_t * mkc_var_list_copy (mkc_varlist_t *varlist, mkc_list_t *list);

MKC_NODISCARD
mkc_varlist_t *
mkc_varlist_init (mkc_log_t *log, mkc_error_t *mkcerr)
{
  mkc_varlist_t  *varlist;

  varlist = malloc (sizeof (mkc_varlist_t));
  if (varlist == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  varlist->list = mkc_list_init (MKC_LIST_SORTED,
      mkc_var_free, mkc_var_compare, mkcerr);
  varlist->debug = false;
  varlist->mkcerr = mkcerr;
  varlist->log = log;
  varlist->fromcache = false;

  return varlist;
}

void
mkc_varlist_free (mkc_varlist_t *varlist)
{
  if (varlist == NULL) {
    return;
  }

  if (varlist->list != NULL) {
    mkc_list_free (varlist->list);
  }

  free (varlist);
}

void
mkc_var_set_fromcache (mkc_varlist_t *varlist, bool flag)
{
  if (varlist == NULL) {
    return;
  }

  varlist->fromcache = flag;
}

int
mkc_var_set (mkc_varlist_t *varlist,
    const char *vname, value_t *value)
{
  mkc_err_code_t  rc = MKC_OK;
  mkc_var_t       *var;
  mkc_varidx_t    vidx;
  value_t     *tvalue;
  value_t     valuecopy;
  value_type_t  nvtype;
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

  /* need a copy of the value, as it may get trashed by a realloc */
  memcpy (&valuecopy, value, sizeof (value_t));
  value = &valuecopy;

  vidx = mkc_var_find (varlist, vname, &loc);
  if (vidx == MKC_VAR_NOTFOUND) {
    var = mkc_var_create (varlist, vname, value->vtype, &loc);
    if (mkc_error_chk_err (varlist->mkcerr)) {
      return MKC_ERR_FAILURE;
    }
  } else {
    var = mkc_list_get_by_idx (varlist->list, vidx);
  }

  tvalue = &var->value;

  nvtype = value->vtype;
  if (value_is_string_type (value)) {
    nvtype = MKC_VT_STRING;
  }

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

  if (tvalue->vtype != MKC_VT_INVALID) {
    value_free (tvalue);
  }

  tvalue->vctxt = value->vctxt;
  tvalue->vtype = nvtype;

  if (nvtype == MKC_VT_STRING) {
    tvalue->sval = strdup (value->sval);
  }
  if (nvtype == MKC_VT_INTEGER) {
    tvalue->ival = value->ival;
  }
  if (nvtype == MKC_VT_TIMESTAMP) {
    tvalue->tmval = value->tmval;
  }
  if (nvtype == MKC_VT_LIST) {
    tvalue->list = mkc_var_list_copy (varlist, value->list);
  }

  return rc;
}

void
mkc_var_set_context (mkc_varlist_t *varlist, const char *vname, int vctxt)
{
  mkc_listidx_t   vidx;
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
  mkc_var_t       *var;
  value_t     *tvalue;

  if (varlist == NULL) {
    return;
  }
  if (vname == NULL) {
    mkc_error_set (varlist->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  vidx = mkc_var_find (varlist, vname, &loc);
  if (vidx == MKC_VAR_NOTFOUND) {
    return;
  }

  var = mkc_list_get_by_idx (varlist->list, vidx);
  tvalue = &var->value;
  tvalue->vctxt = vctxt;
}

int32_t
mkc_var_size (mkc_varlist_t *varlist)
{
  int32_t     sz;

  if (varlist == NULL) {
    return 0;
  }

  sz = mkc_list_size (varlist->list);
  return sz;
}

void
mkc_var_iter_start (mkc_varlist_t *varlist, mkc_varidx_t *iteridx)
{
  if (varlist == NULL || iteridx == NULL) {
    return;
  }

  mkc_list_iter_start (varlist->list, iteridx);
}

mkc_varidx_t
mkc_var_iter_next (mkc_varlist_t *varlist, mkc_varidx_t *iteridx)
{
  mkc_varidx_t   idx;

  if (varlist == NULL || iteridx == NULL) {
    return MKC_ITER_FINISH;
  }

  idx = mkc_list_iter_next (varlist->list, iteridx);
  return idx;
}

value_t *
mkc_var_get_value (mkc_varlist_t *varlist, const char *name)
{
  mkc_varidx_t  vidx = MKC_VAR_NOTFOUND;
  mkc_var_t     *var;
  value_t   *value;
  mkc_listidx_t loc = MKC_LIST_NOTFOUND;

  if (varlist == NULL) {
    return NULL;
  }

  if (mkc_list_size (varlist->list) == 0) {
    return NULL;
  }

  vidx = mkc_var_find (varlist, name, &loc);
  if (vidx == MKC_VAR_NOTFOUND) {
    return NULL;
  }

  var = mkc_list_get_by_idx (varlist->list, vidx);
  if (var == NULL) {
    return NULL;
  }

  value = &var->value;
  return value;
}

value_t *
mkc_var_get_value_by_idx (mkc_varlist_t *varlist, mkc_varidx_t vidx)
{
  mkc_var_t     *var;
  value_t   *value;

  if (varlist == NULL) {
    return 0;
  }

  var = mkc_list_get_by_idx (varlist->list, vidx);
  if (var == NULL) {
    return NULL;
  }
  value = &var->value;

  return value;
}

const char *
mkc_var_get_name (mkc_varlist_t *varlist, mkc_varidx_t vidx)
{
  mkc_var_t     *var;
  const char    *nm;

  if (varlist == NULL) {
    return 0;
  }

  var = mkc_list_get_by_idx (varlist->list, vidx);
  if (var == NULL) {
    return NULL;
  }
  nm = var->name;

  return nm;
}

bool
mkc_var_is_defined (mkc_varlist_t *varlist, const char *vname)
{
  mkc_varidx_t    vidx;
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
  bool            rc = false;

  vidx = mkc_var_find (varlist, vname, &loc);
  if (vidx != MKC_VAR_NOTFOUND) {
    rc = true;
  }

  return rc;
}

bool
mkc_var_is_list (mkc_varlist_t *varlist, const char *vname)
{
  mkc_varidx_t    vidx;
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
  bool            rc = false;

  vidx = mkc_var_find (varlist, vname, &loc);
  if (vidx != MKC_VAR_NOTFOUND) {
    mkc_var_t   *var;
    value_t *value;

    var = mkc_list_get_by_idx (varlist->list, vidx);
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

static mkc_var_t *
mkc_var_create (mkc_varlist_t *varlist,
    const char *name, value_type_t vtype, mkc_listidx_t *loc)
{
  mkc_var_t     *var;
  mkc_var_t     tvar;

  if (name == NULL) {
    mkc_error_set (varlist->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

  tvar.name = strdup (name);
  value_init (&tvar.value);
  tvar.fromcache = varlist->fromcache;

  var = mkc_list_set (varlist->list, &tvar, sizeof (mkc_var_t), loc);

  return var;
}

static mkc_varidx_t
mkc_var_find (mkc_varlist_t *varlist, const char *name, mkc_listidx_t *loc)
{
  mkc_var_t     tvar;
  mkc_varidx_t  rc;

  if (varlist == NULL) {
    return MKC_ERR_FAILURE;
  }

  tvar.name = (char *) name;
  rc = mkc_list_find (varlist->list, &tvar, loc);
  return rc;
}

static void
mkc_var_free (void *data)
{
  mkc_var_t   *var = data;

  if (var == NULL) {
    return;
  }

  free (var->name);
  value_free (&var->value);
}

static int
mkc_var_compare (void *tvara, void *tvarb)
{
  mkc_var_t   *vara = tvara;
  mkc_var_t   *varb = tvarb;

  if (vara == NULL || varb == NULL) {
    return 0;
  }

  return strcmp (vara->name, varb->name);
}

static mkc_list_t *
mkc_var_list_copy (mkc_varlist_t *varlist, mkc_list_t *list)
{
  mkc_list_t      *nlist;
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;
  value_t     *value;
  value_t     nvalue;

  /* the values created in this list are copies, and are not */
  /* present in an astnode, so must be freed */
  nlist = mkc_list_init (MKC_LIST_UNSORTED, value_free, NULL, varlist->mkcerr);
  if (mkc_error_chk_err (varlist->mkcerr)) {
    return NULL;
  }

  mkc_list_iter_start (list, &iteridx);
  while ((lidx = mkc_list_iter_next (list, &iteridx)) != MKC_ITER_FINISH) {
    mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

    if (mkc_error_chk_err (varlist->mkcerr)) {
      break;
    }

    value = mkc_list_get_by_idx (list, lidx);
    memcpy (&nvalue, value, sizeof (value_t));

    if (value_is_string_type (value)) {
      nvalue.vtype = MKC_VT_STRING;
      nvalue.sval = strdup (value->sval);
    }
    if (value->vtype == MKC_VT_LIST) {
      nvalue.list = mkc_var_list_copy (varlist, value->list);
    }
    mkc_list_set (nlist, &nvalue, sizeof (value_t), &loc);
  }

  return nlist;
}

