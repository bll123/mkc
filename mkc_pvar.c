/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "mkc_def.h"
#include "mkc_env.h"
#include "mkc_error.h"
#include "mkc_profile.h"
#include "mkc_pvar.h"
#include "mkc_string.h"
#include "mkc_var.h"

typedef struct mkc_pvar_t {
  mkc_profile_t   * profiles;
  mkc_error_t     * mkcerr;
  mkc_log_t       * log;
  bool            debug;
  bool            fromcache;
} mkc_pvar_t;

static mkc_value_t * mkc_pvar_get_value_by_profidx (mkc_pvar_t *pvar, const char *vname, mkc_profidx_t pidx);

MKC_NODISCARD
mkc_pvar_t *
mkc_pvar_init (mkc_profile_t *profiles, mkc_log_t *log, mkc_error_t *mkcerr)
{
  mkc_pvar_t  *pvar;

  if (profiles == NULL) {
    return NULL;
  }

  pvar = malloc (sizeof (mkc_pvar_t));
  if (pvar == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  pvar->debug = false;
  pvar->mkcerr = mkcerr;
  pvar->log = log;
  pvar->profiles = profiles;
  pvar->fromcache = false;

  return pvar;
}

void
mkc_pvar_free (mkc_pvar_t *pvar)
{
  if (pvar == NULL) {
    return;
  }

  free (pvar);
}

int
mkc_pvar_profile_set (mkc_pvar_t *pvar, const char *pname,
    mkc_compiler_t compiler)
{
  mkc_profidx_t   pidx;

  if (pvar == NULL) {
    return MKC_PROF_NOT_FOUND;
  }
  if (pname == NULL) {
    return MKC_PROF_NOT_FOUND;
  }

  pidx = mkc_profile_find (pvar->profiles, pname, compiler);
  if (pidx != MKC_PROF_NOT_FOUND) {
    mkc_varlist_t  *varlist;

    mkc_profile_set_active (pvar->profiles, pidx);
    varlist = mkc_profile_get_varlist (pvar->profiles, pidx);
    mkc_var_set_fromcache (varlist, pvar->fromcache);
    return MKC_OK;
  }

  return MKC_PROF_NOT_FOUND;
}

int
mkc_pvar_profile_set_idx (mkc_pvar_t *pvar, mkc_profidx_t pidx)
{
  if (pvar == NULL) {
    return MKC_PROF_NOT_FOUND;
  }

  if (pidx != MKC_PROF_NOT_FOUND) {
    mkc_varlist_t  *varlist;

    mkc_profile_set_active (pvar->profiles, pidx);
    varlist = mkc_profile_get_varlist (pvar->profiles, pidx);
    mkc_var_set_fromcache (varlist, pvar->fromcache);
    return MKC_OK;
  }

  return MKC_PROF_NOT_FOUND;
}

void
mkc_pvar_set_fromcache (mkc_pvar_t *pvar, bool flag)
{
  if (pvar == NULL) {
    return;
  }

  pvar->fromcache = flag;
}

const char *
mkc_pvar_name_alloc (mkc_pvar_t *pvar, const char *vname)
{
  const char      *tmp;
  mkc_varlist_t   *varlist;
  mkc_profidx_t   pidx;

  if (pvar == NULL) {
    return NULL;
  }

  pidx = mkc_profile_get_active (pvar->profiles);
  varlist = mkc_profile_get_varlist (pvar->profiles, pidx);
  tmp = mkc_var_name_alloc (varlist, vname);
  return tmp;
}

int
mkc_pvar_set (mkc_pvar_t *pvar, const char *vname,
    mkc_value_t *value, mkc_var_ctxt_t vctxt)
{
  mkc_varlist_t   *varlist;
  int             rc = MKC_ERR_FAILURE;
  mkc_profidx_t   pidx;

  if (pvar == NULL || vname == NULL || value == NULL) {
    return rc;
  }

  pidx = mkc_profile_get_active (pvar->profiles);
  varlist = mkc_profile_get_varlist (pvar->profiles, pidx);
  value->vctxt = vctxt;
  rc = mkc_var_set (varlist, vname, value);

  return rc;
}

int
mkc_pvar_set_integer (mkc_pvar_t *pvar,
    const char *vname, int32_t ival, mkc_var_ctxt_t vctxt)
{
  int         rc = MKC_ERR_FAILURE;
  mkc_value_t value;

  value.ival = ival;
  value.vtype = MKC_VT_INTEGER;

  rc = mkc_pvar_set (pvar, vname, &value, vctxt);
  return rc;
}

int
mkc_pvar_set_str (mkc_pvar_t *pvar,
    const char *vname, const char *str, mkc_var_ctxt_t vctxt)
{
  int         rc = MKC_ERR_FAILURE;
  mkc_value_t value;

  value.sval = (char *) str;
  value.vtype = MKC_VT_STRING;

  rc = mkc_pvar_set (pvar, vname, &value, vctxt);
  return rc;
}

int
mkc_pvar_set_list (mkc_pvar_t *pvar,
    const char *vname, mkc_list_t *list, mkc_var_ctxt_t vctxt)
{
  int         rc = MKC_ERR_FAILURE;
  mkc_value_t value;

  value.list = list;
  value.vtype = MKC_VT_LIST;

  rc = mkc_pvar_set (pvar, vname, &value, vctxt);
  return rc;
}

int
mkc_pvar_set_list_from_str (mkc_pvar_t *pvar,
    const char *vname, char *str, mkc_var_ctxt_t vctxt)
{
  int           rc = MKC_ERR_FAILURE;
  mkc_value_t   value;
  mkc_list_t    *tlist = NULL;
  char          *p;
  char          *tokstr;
  mkc_listidx_t loc;

  tlist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, pvar->mkcerr);
  p = mkc_strtok (str, " ", &tokstr);
  while (p != NULL) {
    if (mkc_error_chk_err (pvar->mkcerr)) {
      return MKC_ERR_FAILURE;
    }

    mkc_strtrim (p, 0);
    value.sval = p;
    value.vtype = MKC_VT_STRING;
    mkc_list_set (tlist, &value, sizeof (mkc_value_t), &loc);
    p = mkc_strtok (NULL, " ", &tokstr);
  }

  value.list = tlist;
  value.vtype = MKC_VT_LIST;
  rc = mkc_pvar_set (pvar, vname, &value, vctxt);

  /* var-set has made a copy of the list */
  mkc_list_free (tlist);

  return rc;
}

void
mkc_pvar_set_context (mkc_pvar_t *pvar, const char *vname,
    mkc_var_ctxt_t vctxt)
{
  mkc_value_t   *value;

  if (pvar == NULL || vname == NULL) {
    return;
  }

  value = mkc_pvar_get_by_profile (pvar, vname);
  if (value != NULL) {
    value->vctxt = vctxt;
  }
}

int32_t
mkc_pvar_size (mkc_pvar_t *pvar)
{
  mkc_varlist_t   *varlist;
  int32_t         sz;
  mkc_profidx_t   pidx;

  if (pvar == NULL) {
    return 0;
  }

  pidx = mkc_profile_get_active (pvar->profiles);
  varlist = mkc_profile_get_varlist (pvar->profiles, pidx);
  sz = mkc_var_size (varlist);
  return sz;
}

void
mkc_pvar_iter_start (mkc_pvar_t *pvar, mkc_varidx_t *iteridx)
{
  mkc_varlist_t   *varlist;
  mkc_profidx_t   pidx;

  if (pvar == NULL || iteridx == NULL) {
    return;
  }

  pidx = mkc_profile_get_active (pvar->profiles);
  varlist = mkc_profile_get_varlist (pvar->profiles, pidx);
  mkc_var_iter_start (varlist, iteridx);
}

mkc_varidx_t
mkc_pvar_iter_next (mkc_pvar_t *pvar, mkc_varidx_t *iteridx)
{
  mkc_varlist_t   *varlist;
  int             rc;
  mkc_profidx_t   pidx;

  if (pvar == NULL || iteridx == NULL) {
    return MKC_ERR_FAILURE;
  }

  pidx = mkc_profile_get_active (pvar->profiles);
  varlist = mkc_profile_get_varlist (pvar->profiles, pidx);
  rc = mkc_var_iter_next (varlist, iteridx);

  return rc;
}

/* get-by-profile searches the profile hierarchy for the variable name */
/* the search hierarchy: */
/*    local-variables */
/*    template         compiler */
/*    current-profile  compiler */
/*    current-profile  general */
/*    internal */
mkc_value_t *
mkc_pvar_get_by_profile (mkc_pvar_t *pvar, const char *nm)
{
  mkc_profidx_t   opidx;
  mkc_profidx_t   pidx;
  mkc_compiler_t  origcompiler;
  mkc_value_t     *value = NULL;
  mkc_profiter_t  profiter;

  if (pvar == NULL) {
    return NULL;
  }
  if (nm == NULL) {
    mkc_error_set (pvar->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

  opidx = mkc_profile_get_active (pvar->profiles);
  origcompiler = mkc_profile_get_compiler (pvar->profiles, opidx);

// ### this isn't correct, the compiler to pass should be the
// specific compiler
  mkc_profile_iter_hierarchy_start (pvar->profiles,
      origcompiler, &profiter);

  while ((pidx = mkc_profile_iter_hierarchy_next (
      pvar->profiles, &profiter)) != MKC_PROF_NOT_FOUND) {
    mkc_var_type_t    vtype = MKC_VT_INVALID;

    value = mkc_pvar_get_value_by_profidx (pvar, nm, pidx);
    if (value == NULL) {
      continue;
    }

    vtype = value->vtype;
    if (vtype == MKC_VT_LIST ||
        vtype == MKC_VT_INTEGER ||
        vtype == MKC_VT_STRING) {
      break;
    }
  }

  mkc_pvar_profile_set_idx (pvar, opidx);
  return value;
}

mkc_value_t *
mkc_pvar_get_by_profidx (mkc_pvar_t *pvar, const char *nm, mkc_profidx_t pidx)
{
  mkc_value_t       *value = NULL;

  if (pvar == NULL) {
    return NULL;
  }
  if (nm == NULL) {
    mkc_error_set (pvar->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

  value = mkc_pvar_get_value_by_profidx (pvar, nm, pidx);
  return value;
}

mkc_value_t *
mkc_pvar_get_value (mkc_pvar_t *pvar, const char *vname)
{
  mkc_varlist_t   *varlist;
  mkc_value_t     *value;
  mkc_profidx_t   pidx;

  if (pvar == NULL) {
    return NULL;
  }


  pidx = mkc_profile_get_active (pvar->profiles);
  varlist = mkc_profile_get_varlist (pvar->profiles, pidx);
  value = mkc_var_get_value (varlist, vname);
  return value;
}

mkc_value_t *
mkc_pvar_get_by_idx (mkc_pvar_t *pvar, mkc_varidx_t vidx)
{
  mkc_varlist_t   *varlist;
  mkc_value_t     *value;
  mkc_profidx_t   pidx;

  if (pvar == NULL) {
    return NULL;
  }

  pidx = mkc_profile_get_active (pvar->profiles);
  varlist = mkc_profile_get_varlist (pvar->profiles, pidx);
  value = mkc_var_get_value_by_idx (varlist, vidx);
  return value;
}

const char *
mkc_pvar_get_name (mkc_pvar_t *pvar, mkc_varidx_t vidx)
{
  mkc_varlist_t   *varlist;
  const char      *name;
  mkc_profidx_t   pidx;

  if (pvar == NULL) {
    return NULL;
  }

  pidx = mkc_profile_get_active (pvar->profiles);
  varlist = mkc_profile_get_varlist (pvar->profiles, pidx);
  name = mkc_var_get_name (varlist, vidx);
  return name;
}

void
mkc_pvar_get_env_str (mkc_pvar_t *pvar, const char *envstr,
    char *buff, size_t sz)
{
  *buff = '\0';

  if (pvar == NULL) {
    return;
  }

  mkc_env_get (envstr, buff, sz);
}

int32_t
mkc_pvar_get_variable_integer (mkc_pvar_t *pvar, mkc_value_t *value)
{
  int32_t       ival = 0;
  mkc_value_t   *tvalue;

  tvalue = mkc_pvar_get_variable_value (pvar, value->sval);
  if (tvalue == NULL) {
    ival = 0;
    return ival;
  }
  if (tvalue->vtype == MKC_VT_STRING) {
    ival = atol (tvalue->sval);
  }
  if (tvalue->vtype == MKC_VT_INTEGER) {
    ival = tvalue->ival;
  }

  return ival;
}

void
mkc_pvar_get_variable_str (mkc_pvar_t *pvar, mkc_value_t *value,
    char *buff, size_t sz)
{
  mkc_value_t     *tvalue;

  if (pvar == NULL) {
    return;
  }

  *buff = '\0';

  if (value->vtype == MKC_VT_ENV_VARIABLE) {
    mkc_pvar_get_env_str (pvar, value->sval, buff, sz);
    return;
  }

  tvalue = mkc_pvar_get_variable_value (pvar, value->sval);

  if (tvalue == NULL) {
    return;
  }

  if (tvalue->vtype == MKC_VT_STRING) {
    stpecpy (buff, buff + sz, tvalue->sval);
  }
  if (tvalue->vtype == MKC_VT_INTEGER) {
    snprintf (buff, sz, "%d", tvalue->ival);
  }

  {
    char    dbuff [MKC_PATH_MAX];

    mkc_log (pvar->log, MKC_LOG_PROCESS, "  pvar-v-get-str-var: %s\n",
        mkc_value_to_str (tvalue, dbuff, sizeof (dbuff)));
  }
}

mkc_value_t *
mkc_pvar_get_variable_value (mkc_pvar_t *pvar, const char *str)
{
  char        *tstr;
  mkc_value_t *value;

  tstr = mkc_pvar_substitute (pvar, str, 0);
  value = mkc_pvar_get_by_profile (pvar, tstr);
  free (tstr);
  return value;
}

void
mkc_pvar_debug (mkc_pvar_t *pvar)
{
  if (pvar == NULL) {
    return;
  }

  pvar->debug = ! pvar->debug;
}

int32_t
mkc_pvar_value_get_integer (mkc_pvar_t *pvar, mkc_value_t *value)
{
  int32_t     ival = 0;

  if (value->vtype == MKC_VT_INVALID) {
    mkc_error_set (pvar->mkcerr, MKC_ERR_INVALID_VALUE, 0, NULL);
  } else if (value->vtype == MKC_VT_INTEGER) {
    ival = value->ival;
  } else if (value->vtype == MKC_VT_STATIC_STRING) {
// ### conversion from string to integer is not necessary
//  these could be removed, and just generate an error.
    ival = atol (value->sval);
  } else if (value->vtype == MKC_VT_QUOTED_STRING) {
    char    *tbuff;

    tbuff = mkc_pvar_substitute (pvar, value->sval, 0);
    ival = atol (tbuff);
    free (tbuff);
  } else if (value->vtype == MKC_VT_LIST) {
    mkc_error_set (pvar->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT, 0, NULL);
    return 0;
  } else if (value->vtype == MKC_VT_ENV_VARIABLE) {
    char    tbuff [MKC_PATH_MAX];

    mkc_pvar_get_env_str (pvar, value->sval, tbuff, sizeof (tbuff));
    ival = atol (tbuff);
  } else if (value->vtype == MKC_VT_VARIABLE) {
    ival = mkc_pvar_get_variable_integer (pvar, value);
  } else if (value->vtype == MKC_VT_STRING) {
// ### conversion from string to integer is not necessary
//  these could be removed, and just generate an error.
    ival = atol (value->sval);
  } else {
    mkc_error_set (pvar->mkcerr, MKC_ERR_UNHANDLED_VALUE, 0, NULL);
    fprintf (stderr, "ERR: unhandled value: %d\n", value->vtype);
  }

  mkc_log (pvar->log, MKC_LOG_PROCESS, "  pv-get-int: %d\n", ival);
  return ival;
}

void
mkc_pvar_value_get_str (mkc_pvar_t *pvar,
    mkc_value_t *value, char *buff, size_t sz)
{
  *buff = '\0';

  if (value->vtype == MKC_VT_INVALID) {
    mkc_error_set (pvar->mkcerr, MKC_ERR_INVALID_VALUE, 0, NULL);
  } else if (value->vtype == MKC_VT_INTEGER) {
    snprintf (buff, sz, "%d", value->ival);
  } else if (value->vtype == MKC_VT_STRING) {
    stpecpy (buff, buff + sz, value->sval);
  } else if (value->vtype == MKC_VT_STATIC_STRING) {
    stpecpy (buff, buff + sz, value->sval);
  } else if (value->vtype == MKC_VT_QUOTED_STRING) {
    char    *tbuff;

    tbuff = mkc_pvar_substitute (pvar, value->sval, 0);
    stpecpy (buff, buff + sz, tbuff);
    free (tbuff);
  } else if (value->vtype == MKC_VT_LIST) {
    mkc_error_set (pvar->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT, 0, NULL);
  } else if (value->vtype == MKC_VT_ENV_VARIABLE) {
    mkc_pvar_get_env_str (pvar, value->sval, buff, sz);
  } else if (value->vtype == MKC_VT_VARIABLE) {
    mkc_pvar_get_variable_str (pvar, value, buff, sz);
  } else {
    mkc_error_set (pvar->mkcerr, MKC_ERR_UNHANDLED_VALUE, 0, NULL);
    fprintf (stderr, "ERR: unhandled value: %d\n", value->vtype);
  }

  mkc_log (pvar->log, MKC_LOG_PROCESS, "  pv-get-str: %s\n", buff);
}

bool
mkc_pvar_is_defined (mkc_pvar_t *pvar, const char *vname)
{
  mkc_value_t     *value;
  bool            rc = false;

  if (pvar == NULL) {
    return rc;
  }

  value = mkc_pvar_get_by_profile (pvar, vname);
  if (value == NULL) {
    return rc;
  }
  return rc;
}

bool
mkc_pvar_is_list (mkc_pvar_t *pvar, const char *vname)
{
  mkc_value_t     *value;
  bool            rc = false;

  if (pvar == NULL) {
    return rc;
  }

  value = mkc_pvar_get_by_profile (pvar, vname);
  if (value == NULL) {
    return rc;
  }

  if (value->vtype == MKC_VT_LIST) {
    rc = true;
  }
  return rc;
}

/* processes the internal substitutions */
/* if the string is a variable, the final substitution is done */
/* by the caller by calling mkc_pvar_get_by_profile () */
char *
mkc_pvar_substitute (mkc_pvar_t *pvar, const char *data, int depth)
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

  if (pvar == NULL) {
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
        mkc_error_set (pvar->mkcerr, MKC_ERR_UNBALANCED_BRACES, 0, NULL);
        fprintf (stderr, "ERROR: unbalanced braces '%s'\n", data);
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
      tstr = mkc_pvar_substitute (pvar, substr, depth + 1);
      free (substr);
//fprintf (stderr, "%*ststr: '%s'\n", depth * 2, "", tstr);

      if (isenv) {
        mkc_pvar_get_env_str (pvar, tstr, ebuff, sizeof (ebuff));
        tval = ebuff;
      } else {
        mkc_value_t   *value;

        value = mkc_pvar_get_by_profile (pvar, tstr);
//fprintf (stderr, "%*svalue-null %d\n", depth * 2, "", value == NULL ? 1 : 0);
        if (value != NULL && value->vtype == MKC_VT_INTEGER) {
          snprintf (tbuff, sizeof (tbuff), "%d", value->ival);
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
  return buff;
}

static mkc_value_t *
mkc_pvar_get_value_by_profidx (mkc_pvar_t *pvar,
    const char *vname, mkc_profidx_t pidx)
{
  mkc_varlist_t   *varlist;
  mkc_value_t     *value;

  if (pvar == NULL) {
    return NULL;
  }

  varlist = mkc_profile_get_varlist (pvar->profiles, pidx);
  value = mkc_var_get_value (varlist, vname);
  return value;
}

