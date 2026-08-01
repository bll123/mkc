/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#ifndef MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "alternate.h"
#include "attribute.h"
#include "chararr.h"
#include "comptest.h"
#include "fileop.h"
#include "mkc_compiler.h"
#include "mkc_const.h"
#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_util.h"
#include "mkc_util.h"
#include "os_process.h"
#include "scopedvar.h"
#include "strutil.h"
#include "target.h"
#include "tmutil.h"

typedef struct target_t {
  scopedvar_t     * scopedvar;
  comptest_t      * comptest;
  mkc_attribute_t * attr;
  mkc_log_t       * log;
  mkc_error_t     * mkcerr;
} target_t;

static bool target_chk_last_libloc (char *lastlibloc, size_t sz, const char *str);

target_t *
target_init (scopedvar_t *scopedvar, comptest_t *comptest,
    mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr)
{
  target_t    *target;

  target = malloc (sizeof (target_t));
  if (target == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  target->scopedvar = scopedvar;
  target->comptest = comptest;
  target->attr = attr;
  target->log = log;
  target->mkcerr = mkcerr;

  return target;
}

void
target_free (target_t *target)
{
  if (target == NULL) {
    return;
  }

  free (target);
}

chararr_t *
target_get_flags (target_t *target, const char *flagname)
{
  mkc_list_t      * tlist;
  char            * lastlibloc;
  char            * str;
  scopedvar_t     * scopedvar;
  sv_iter_t       * sviter = NULL;
  const char      * profnm;
  chararr_t       * flags;

  lastlibloc = malloc (MKC_PATH_MAX);
  if (lastlibloc == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  *lastlibloc = '\0';

  str = malloc (MKC_PATH_MAX);
  if (str == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  *str = '\0';

  tlist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, target->mkcerr);

  scopedvar = target->scopedvar;

  flags = chararr_init (target->mkcerr);
  if (flags == NULL) {
    return NULL;
  }
  chararr_freeinternals (flags);

  sviter = scopedvar_iter_start (scopedvar, SV_ITER_HIERARCHY);
  while ((profnm = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    value_t         *value = NULL;
    mkc_listidx_t   fiter;
    mkc_listidx_t   fidx;
    sv_type_t       svtype;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    svtype = scopedvar_iter_get_type (scopedvar, sviter);
    value = scopedvar_get_value (scopedvar, svtype, flagname);
    if (value == NULL || value->vtype != MKC_VT_LIST) {
      continue;
    }

    mkc_list_iter_start (value->list, &fiter);
    while ((fidx = mkc_list_iter_next (value->list, &fiter)) != MKC_ITER_FINISH) {
      value_t   *fval;

      if (mkc_error_chk_err (target->mkcerr)) {
        break;
      }

      fval = mkc_list_get_by_idx (value->list, fidx);
      scopedvar_value_get_str (scopedvar, fval, str, MKC_PATH_MAX);
      if (! *str) {
        continue;
      }
      if (target_chk_last_libloc (lastlibloc, MKC_PATH_MAX, str)) {
        /* de-duplication check */
        continue;
      }

      chararr_append (flags, strdup (str));
    }
  }
  scopedvar_iter_finish (sviter);
  chararr_append (flags, NULL);

  mkc_list_free (tlist);
  free (lastlibloc);
  free (str);

  return flags;
}

void
target_get_dependencies (target_t *target,
    mkc_compiler_t compiler, const char *filename, const char *filepath)
{
  int             rc;
  int64_t         fts;
  mkc_list_t      * elist;
  char            * rbuff;
  size_t          rsz;
  int64_t         currtm;
  chararr_t       * cflags;
  char            * tokstr;
  char            * p;
  bool            first = true;

  currtm = mstime ();
  fts = 0;
  if (scopedvar_is_defined (target->scopedvar, SV_T_TIMESTAMP, filename)) {
    fts = scopedvar_get_timestamp (target->scopedvar, SV_T_TIMESTAMP, filename);
  }
  if (currtm < fts &&
      scopedvar_is_defined (target->scopedvar, SV_T_DEPENDENCY, filename)) {
    return;
  }

  rsz = MKC_LARGE_BUFF_SZ;
  rbuff = malloc (rsz);
  if (rbuff == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  *rbuff = '\0';

  cflags = target_get_flags (target, MKC_C_CFLAGS);
  comptest_append_compflag (target->comptest, "-M");
  comptest_append_compflag (target->comptest, NULL);
  comptest_preprocess (target->comptest);
  comptest_set_flags (target->comptest, cflags, NULL, NULL);
  rc = comptest_test (target->comptest, MKC_COMPILE_ONLY, compiler,
      filepath, rbuff, rsz);
  comptest_reset (target->comptest);
  if (rc != MKC_OK) {
    return;
  }

  /* note that the lists loaded from the cache are not sorted */
  /* since they are supposedly complete, they should not need to be searched */
  /* and processing should work.  this may need to be re-visited later */

  /* any existing list must be cleared, so set the variable to an empty list */

  elist = mkc_list_init (MKC_LIST_UNSORTED, NULL, value_str_compare, target->mkcerr);
  scopedvar_set_list (target->scopedvar, SV_T_DEPENDENCY,
      filename, elist, MKC_VCTXT_MKC);
  mkc_list_free (elist);

  p = str_token (rbuff, " ", &tokstr);

  while (p != NULL) {
    char    *tp;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    if (*p != '\\' && ! first) {
      str_trim (p, 0);
      tp = strrchr (p, '/');
      if (tp == NULL) {
        tp = p;
      } else {
        tp += 1;
      }
      if (strcmp (filename, tp) != 0) {
        scopedvar_append_str_list (target->scopedvar, SV_T_DEPENDENCY,
            filename, p, MKC_VCTXT_MKC);
      }
    }
    p = str_token (NULL, " ", &tokstr);
    first = false;
  }

  /* set the timestamp when the file is processed */
  /* convert to ms for comparison to the real time */
  fts = fileop_modtime (filepath);
  fts *= 1000;
  scopedvar_set_timestamp (target->scopedvar, SV_T_TIMESTAMP, filename, fts, MKC_VCTXT_MKC);

  free (rbuff);
  chararr_free (cflags);
}

/* internal routines */

static bool
target_chk_last_libloc (char *lastlibloc, size_t sz, const char *str)
{
  if (! mkc_flag_is_libloc (str)) {
    return false;
  }

  if (strcmp (lastlibloc, str) == 0) {
    return true;
  }

  stpecpy (lastlibloc, lastlibloc + sz, str);
  return false;
}

