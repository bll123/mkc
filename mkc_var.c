/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_var.h"
#include "mkc_string.h"

typedef struct mkc_var_t {
  char          * name;
  mkc_value_t   value;
  bool          fromcache;
} mkc_var_t;

typedef struct mkc_varlist_t {
  mkc_list_t    * list;
  mkc_error_t   * mkcerr;
  mkc_log_t     * log;
  char          ** names;
  int32_t       name_allocsz;
  int32_t       name_sz;
  bool          debug;
  bool          fromcache;
} mkc_varlist_t;

static char const * const vctxtnames [] = {
  [MKC_VCTXT_CHECK] = "check",
  [MKC_VCTXT_ENV] = "env",
  [MKC_VCTXT_FLAG] = "flag",
  [MKC_VCTXT_MKC] = "mkc",
  [MKC_VCTXT_TEMP] = "temp",
  [MKC_VCTXT_USER_DISABLE] = "disable",
  [MKC_VCTXT_USER_ENABLE] = "enable",
};

static mkc_var_t *mkc_var_create (mkc_varlist_t *varlist, const char *vname, mkc_var_type_t type, mkc_listidx_t *loc);
static mkc_varidx_t mkc_var_find (mkc_varlist_t *varlist, const char *name, mkc_listidx_t *loc);
static void mkc_var_free (void *data);
static int mkc_var_compare (void *tvara, void *tvarb);
static mkc_list_t * mkc_var_list_copy (mkc_varlist_t *varlist, mkc_list_t *list);

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
  varlist->names = NULL;
  varlist->name_allocsz = 0;
  varlist->name_sz = 0;
  varlist->debug = false;
  varlist->mkcerr = mkcerr;
  varlist->log = log;

  return varlist;
}

void
mkc_varlist_free (mkc_varlist_t *varlist)
{
  if (varlist == NULL) {
    return;
  }

  if (varlist->names != NULL) {
    for (int i = 0; i < varlist->name_sz; ++i) {
      free (varlist->names [i]);
    }
    free (varlist->names);
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

const char *
mkc_var_vctxt_str (mkc_var_ctxt_t vctxt)
{
  const char  * vctxtstr;

  vctxtstr = vctxtnames [vctxt];
  return vctxtstr;
}


const char *
mkc_var_name_alloc (mkc_varlist_t *varlist, const char *vname)
{
  int32_t       idx;

  if (varlist == NULL || vname == NULL) {
    return NULL;
  }

  if (varlist->name_sz >= varlist->name_allocsz) {
    varlist->name_allocsz += 10;
    varlist->names = realloc (varlist->names,
        sizeof (char *) * varlist->name_allocsz);
    if (varlist->names == NULL) {
      mkc_error_set (varlist->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return NULL;
    }
  }

  idx = varlist->name_sz;
  varlist->names [idx] = strdup (vname);
  if (varlist->names [idx] == NULL) {
    mkc_error_set (varlist->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  varlist->name_sz += 1;

  return varlist->names [idx];
}

int
mkc_var_set (mkc_varlist_t *varlist,
    const char *name, mkc_value_t *value)
{
  mkc_err_code_t  rc = MKC_OK;
  mkc_var_t       *var;
  mkc_varidx_t    vidx;
  mkc_value_t     *tvalue;
  mkc_var_type_t  nvtype;
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

  vidx = mkc_var_find (varlist, name, &loc);
  if (vidx == MKC_VAR_NOTFOUND) {
    var = mkc_var_create (varlist, name, value->vtype, &loc);
    if (mkc_error_chk_err (varlist->mkcerr)) {
      return MKC_ERR_FAILURE;
    }
  } else {
    var = mkc_list_get_by_idx (varlist->list, vidx);
  }

  tvalue = &var->value;

  nvtype = value->vtype;
  if (value->vtype == MKC_VT_STATIC_STRING ||
      value->vtype == MKC_VT_VARIABLE ||
      value->vtype == MKC_VT_ENV_VARIABLE ||
      value->vtype == MKC_VT_QUOTED_STRING) {
    nvtype = MKC_VT_STRING;
  }

  /* check to see if a variable from the cache has changed */
  /* only do this if the cache is currently loading */
  if (varlist->fromcache && var->fromcache != varlist->fromcache) {
    if (tvalue->vtype == MKC_VT_STRING && nvtype == MKC_VT_STRING) {
      if (strcmp (tvalue->sval, value->sval) != 0) {
        rc = MKC_OK_CHANGE;
      }
    }
    if (tvalue->vtype == MKC_VT_INTEGER && nvtype == MKC_VT_INTEGER) {
      if (tvalue->ival != value->ival) {
        rc = MKC_OK_CHANGE;
      }
    }
  }

  if (tvalue->vtype == MKC_VT_STRING && tvalue->sval != NULL) {
    free (tvalue->sval);
  }
  if (tvalue->vtype == MKC_VT_LIST && tvalue->list != NULL) {
    mkc_list_free (tvalue->list);
  }

  tvalue->vctxt = value->vctxt;
  tvalue->vtype = nvtype;
  if (nvtype == MKC_VT_STRING) {
    tvalue->sval = strdup (value->sval);
  }
  if (nvtype == MKC_VT_INTEGER) {
    tvalue->ival = value->ival;
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
  mkc_listidx_t   loc;
  mkc_var_t       *var;
  mkc_value_t     *tvalue;

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

mkc_value_t *
mkc_var_get_value (mkc_varlist_t *varlist, const char *name)
{
  mkc_varidx_t  vidx = MKC_VAR_NOTFOUND;
  mkc_var_t     *var;
  mkc_value_t   *value;
  mkc_listidx_t loc = MKC_LIST_NOTFOUND;

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

mkc_value_t *
mkc_var_get_value_by_idx (mkc_varlist_t *varlist, mkc_varidx_t vidx)
{
  mkc_var_t     *var;
  mkc_value_t   *value;

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
    mkc_value_t *value;

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

void
mkc_value_free (void *tvalue)
{
  mkc_value_t *value = tvalue;

  if (value == NULL) {
    return;
  }

  if (value->vtype == MKC_VT_LIST && value->list != NULL) {
    mkc_list_free (value->list);
  }
  if (value->vtype == MKC_VT_STRING && value->sval != NULL) {
    free (value->sval);
  }
  if (value->vtype == MKC_VT_VARIABLE && value->sval != NULL) {
    free (value->sval);
  }
  if (value->vtype == MKC_VT_ENV_VARIABLE && value->sval != NULL) {
    free (value->sval);
  }
}

const char *
mkc_value_to_str (mkc_value_t *value, char *buff, size_t sz)
{
  if (value == NULL) {
    snprintf (buff, sz, "null ");
    return buff;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      snprintf (buff, sz, "invalid ");
      break;
    }
    case MKC_VT_INTEGER: {
      snprintf (buff, sz, "i:%d ", value->ival);
      break;
    }
    case MKC_VT_STRING: {
      snprintf (buff, sz, "s:%s ", value->sval);
      break;
    }
    case MKC_VT_LIST: {
      mkc_list_t    *tlist;
      mkc_listidx_t iteridx;
      mkc_listidx_t lidx;
      mkc_value_t   *tvalue;
      char          tbuff [MKC_PATH_MAX];
      char          *p;

      p = stpecpy (buff, buff + sz, "[");
      tlist = value->list;
      mkc_list_iter_start (tlist, &iteridx);
      while ((lidx = mkc_list_iter_next (tlist, &iteridx)) != MKC_ITER_FINISH) {
        tvalue = mkc_list_get_by_idx (tlist, lidx);
        mkc_value_to_str (tvalue, tbuff, sizeof (tbuff));
        p = stpecpy (p, buff + sz, tbuff);
      }
      p = stpecpy (p, buff + sz, "] ");
      break;
    }
    case MKC_VT_VARIABLE: {
      snprintf (buff, sz, "var:%s ", value->sval);
      break;
    }
    case MKC_VT_ENV_VARIABLE: {
      snprintf (buff, sz, "evar:%s ", value->sval);
      break;
    }
    case MKC_VT_STATIC_STRING: {
      snprintf (buff, sz, "ss:%s ", value->sval);
      break;
    }
    case MKC_VT_QUOTED_STRING: {
      snprintf (buff, sz, "qs:%s ", value->sval);
      break;
    }
  }

  return buff;
}

/* internal routines */

static mkc_var_t *
mkc_var_create (mkc_varlist_t *varlist,
    const char *name, mkc_var_type_t vtype, mkc_listidx_t *loc)
{
  mkc_var_t     *var;
  mkc_var_t     tvar;

  if (name == NULL) {
    mkc_error_set (varlist->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

  tvar.name = strdup (name);
  tvar.value.sval = NULL;
  tvar.value.vtype = MKC_VT_INVALID;
  tvar.fromcache = varlist->fromcache;

  var = mkc_list_set (varlist->list, &tvar, sizeof (mkc_var_t), loc);

  return var;
}

static mkc_varidx_t
mkc_var_find (mkc_varlist_t *varlist, const char *name, mkc_listidx_t *loc)
{
  mkc_var_t     tvar;
  mkc_varidx_t  rc;

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
  mkc_value_free (&var->value);
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
  mkc_value_t     *value;
  mkc_value_t     nvalue;

  nlist = mkc_list_init (MKC_LIST_UNSORTED, mkc_value_free,
      NULL, varlist->mkcerr);
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
    memcpy (&nvalue, value, sizeof (mkc_value_t));

    /* everything other than invalid, integers and lists is a string */
    if (nvalue.vtype != MKC_VT_INVALID &&
        nvalue.vtype != MKC_VT_INTEGER &&
        nvalue.vtype != MKC_VT_LIST) {
      nvalue.sval = strdup (value->sval);
    }
    if (nvalue.vtype == MKC_VT_LIST) {
      nvalue.list = mkc_var_list_copy (varlist, value->list);
    }
    mkc_list_set (nlist, &nvalue, sizeof (mkc_value_t), &loc);
  }

  return nlist;
}

