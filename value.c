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
#include "mkc_list.h"
#include "strutil.h"
#include "value.h"

static char const * const valuetypenames [] = {
  [MKC_VT_INVALID] = "invalid",
  [MKC_VT_INTEGER] = "integer",
  [MKC_VT_STRING] = "string",
  [MKC_VT_LIST] = "list",
  [MKC_VT_RANGE] = "range",
  [MKC_VT_STATIC_STRING] = "static_string",
  [MKC_VT_TIMESTAMP] = "timestamp",
  [MKC_VT_QUOTED_STRING] = "quoted_string",
  [MKC_VT_VARIABLE] = "variable",
  [MKC_VT_ENV_VARIABLE] = "env_variable",
};

static char const * const valuectxtnames [MKC_VCTXT_MAX] = {
  [MKC_VCTXT_CHECK] = "check",
  [MKC_VCTXT_ENV] = "env",
  [MKC_VCTXT_FLAG] = "flag",
  [MKC_VCTXT_MKC] = "mkc",
  [MKC_VCTXT_MKC_BASE] = "mkc_base",
  [MKC_VCTXT_TEMP] = "temp",
  [MKC_VCTXT_USER_DISABLE] = "disable",
  [MKC_VCTXT_USER_ENABLE] = "enable",
  [MKC_VCTXT_UNKNOWN] = "unknown",
};

void
value_init (value_t *value)
{
  value->vtype = MKC_VT_INVALID;
  value->sval = NULL;
  value->vctxt = MKC_VCTXT_TEMP;
  value->tempallocated = false;
}

void
value_free (void *tvalue)
{
  value_t *value = tvalue;

  if (value == NULL) {
    return;
  }

  if (value->vtype == MKC_VT_INVALID) {
    return;
  }

  if (value->vtype == MKC_VT_LIST && value->list != NULL) {
    mkc_list_free (value->list);
  }
  if (value_is_string_type (value) && value->sval != NULL) {
    free (value->sval);
  }
  value->vtype = MKC_VT_INVALID;
}

const char *
value_to_str (value_t *value, char *buff, size_t sz)
{
  if (value == NULL) {
    snprintf (buff, sz, "null");
    return buff;
  }

  switch (value->vtype) {
    case MKC_VT_INVALID: {
      snprintf (buff, sz, "invalid");
      break;
    }
    case MKC_VT_RANGE: {
      snprintf (buff, sz, "%" PRId32 "..%" PRId32,
          value->range.beg, value->range.end);
      break;
    }
    case MKC_VT_INTEGER: {
      snprintf (buff, sz, "%" PRId32, value->ival);
      break;
    }
    case MKC_VT_TIMESTAMP: {
      snprintf (buff, sz, "%" PRId64 "LL", value->tmval);
      break;
    }
    case MKC_VT_LIST: {
      mkc_list_t    *tlist;
      mkc_listidx_t iteridx;
      mkc_listidx_t lidx;
      value_t       *tvalue;
      char          tbuff [MKC_PATH_MAX];
      char          *p;
      char          *eptr;
      char          *tp;
      bool          newline = false;
      int           limit = 50;

      tp = buff;
      eptr = buff + sz;
      p = stpecpy (buff, eptr, "[");
      tlist = value->list;
      mkc_list_iter_start (tlist, &iteridx);
      while ((lidx = mkc_list_iter_next (tlist, &iteridx)) != MKC_ITER_FINISH) {
        if (p - tp > limit) {
          p = stpecpy (p, eptr, "\n");
          newline = true;
          tp = p;
          limit = 65;
        }
        tvalue = mkc_list_get_by_idx (tlist, lidx);
        p = stpecpy (p, eptr, " ");
        if (newline) {
          p = stpecpy (p, eptr, "       ");
          newline = false;
        }
        value_to_str (tvalue, tbuff, sizeof (tbuff));
        if (value_is_string_type (tvalue)) {
          p = stpecpy (p, eptr, "'");
        }
        p = stpecpy (p, eptr, tbuff);
        if (value_is_string_type (tvalue)) {
          p = stpecpy (p, eptr, "'");
        }
      }
      p = stpecpy (p, eptr, " ]");
      break;
    }
    case MKC_VT_STRING:
    case MKC_VT_VARIABLE:
    case MKC_VT_ENV_VARIABLE:
    case MKC_VT_STATIC_STRING:
    case MKC_VT_QUOTED_STRING: {
      snprintf (buff, sz, "%s", value->sval);
      break;
    }
  }

  return buff;
}

void
value_range_init (value_t *value,
    int32_t beg, int32_t end, int32_t incr)
{
  if (value == NULL) {
    return;
  }

  value->vtype = MKC_VT_RANGE;
  value->range.finish = false;
  value->range.beg = beg;
  value->range.end = end;
  value->range.incr = incr;
}

void
value_range_iter_start (value_t *value, mkc_listidx_t *iteridx)
{
  if (value == NULL) {
    return;
  }

  *iteridx = value->range.beg - value->range.incr;
}

int
value_range_iter_next (value_t *value, value_t *rval,
    mkc_listidx_t *iteridx)
{
  *iteridx += value->range.incr;
  if (*iteridx >= value->range.end) {
    *iteridx = MKC_ITER_FINISH;
  }

  rval->vtype = MKC_VT_INTEGER;
  rval->ival = *iteridx;
  return *iteridx;
}

void
value_iter_start (value_t *value, mkc_listidx_t *iteridx)
{
  *iteridx = MKC_ITER_FINISH;

  if (value == NULL) {
    return;
  }

  if (value->vtype == MKC_VT_LIST) {
    mkc_list_iter_start (value->list, iteridx);
  }
  if (value->vtype == MKC_VT_RANGE) {
    value_range_iter_start (value, iteridx);
  }
}

int
value_iter_next (value_t *value,
    value_t *rval, mkc_listidx_t *iteridx)
{
  mkc_listidx_t   rc = MKC_ITER_FINISH;

  if (value == NULL) {
    return rc;
  }

  if (value->vtype == MKC_VT_LIST) {
    mkc_listidx_t   lidx;
    value_t         *tvalue;

    lidx = mkc_list_iter_next (value->list, iteridx);
    rc = lidx;
    if (lidx != MKC_ITER_FINISH) {
      tvalue = mkc_list_get_by_idx (value->list, lidx);
      memcpy (rval, tvalue, sizeof (value_t));
    }
  }
  if (value->vtype == MKC_VT_RANGE) {
    rc = value_range_iter_next (value, rval, iteridx);
  }

  return rc;
}

bool
value_is_string_type (const value_t *value)
{
  /* everything other than invalid, integers, */
  /* timestamps and lists is a string */
  if (value->vtype == MKC_VT_STRING ||
      value->vtype == MKC_VT_STATIC_STRING ||
      value->vtype == MKC_VT_QUOTED_STRING ||
      value->vtype == MKC_VT_VARIABLE ||
      value->vtype == MKC_VT_ENV_VARIABLE) {
    return true;
  }

  return false;
}

const char *
value_disp_type (value_t *value)
{
  if (value == NULL) {
    return "null";
  }

  return valuetypenames [value->vtype];
}

const char *
value_ctxt_str (value_ctxt_t vctxt)
{
  const char  * vctxtstr;

  vctxtstr = valuectxtnames [vctxt];
  return vctxtstr;
}

value_ctxt_t
value_ctxt_value (const char *vctxtstr)
{
  value_ctxt_t    vctxt = MKC_VCTXT_UNKNOWN;

  for (int i = 0; i < MKC_VCTXT_MAX; ++i) {
    if (strcmp (valuectxtnames [i], vctxtstr) == 0) {
      vctxt = i;
      return vctxt;
    }
  }

  return vctxt;
}


int
value_str_compare (void *tvala, void *tvalb)
{
  value_t   *vala = tvala;
  value_t   *valb = tvalb;

  if (vala == NULL || valb == NULL) {
    fprintf (stderr, "ERR: value_str_compare: null\n");
    return 0;
  }

  if (! value_is_string_type (vala) ||
      ! value_is_string_type (valb)) {
    fprintf (stderr, "ERR: value_str_compare: comparison of non-string values\n");
    return 0;
  }

  return strcmp (vala->sval, valb->sval);
}
