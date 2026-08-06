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
#include "dirmatch.h"
#include "fileop.h"
#include "mkc_compiler.h"
#include "mkc_const.h"
#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_util.h"
#include "mkc_util.h"
#include "os_process.h"
#include "pathutil.h"
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

static const char * const dependency_delim = " \n\r\\";

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

int
target_check_dependency_timestamp (target_t *target,
    const char *filename, const char *filepath)
{
  int64_t       fts;
  int64_t       ts;
  mkc_listidx_t iteridx;

  if (! scopedvar_is_defined (target->scopedvar, SV_T_DEPENDENCY, filename)) {
    return TARGET_OUT_OF_DATE;
  }

  fts = 0;
  if (scopedvar_is_defined (target->scopedvar, SV_T_TIMESTAMP, filename)) {
    fts = scopedvar_get_timestamp (target->scopedvar, SV_T_TIMESTAMP, filename);
  }

  target_iter_dependency_ts_start (target, filename, &iteridx);
  while ((ts = target_iter_dependency_ts (target, filename, &iteridx)) != MKC_ITER_FINISH) {
    if (ts > fts) {
      return TARGET_OUT_OF_DATE;
    }
  }

  return TARGET_CURRENT;
}

void
target_get_dependencies (target_t *target,
    mkc_compiler_t compiler, const char *filename, const char *filepath,
    target_flag_t flags)
{
  int             rc;
  char            * rbuff;
  size_t          rsz;
  chararr_t       * cflags;
  char            * tokstr;
  char            * p;
  bool            first = true;

  rsz = MKC_LARGE_BUFF_SZ;
  rbuff = malloc (rsz);
  if (rbuff == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  *rbuff = '\0';

  cflags = target_get_flags (target, MKC_C_CFLAGS);
  if ((flags & TARGET_SUPPORTS_MM) == TARGET_SUPPORTS_MM) {
    comptest_append_compflag (target->comptest, "-MM");
  } else {
    comptest_append_compflag (target->comptest, "-M");
  }
  comptest_append_compflag (target->comptest, NULL);
  comptest_preprocess (target->comptest);
  comptest_set_flags (target->comptest, cflags, NULL, NULL);
  rc = comptest_test (target->comptest, MKC_COMPILE_ONLY, compiler,
      filepath, rbuff, rsz);
  comptest_reset (target->comptest);
  if (rc != MKC_OK) {
    free (rbuff);
    chararr_free (cflags);
    return;
  }

  /* note that the lists loaded from the cache are not sorted */
  /* since they are supposedly complete, they should not need to be searched */
  /* and processing should work.  this may need to be re-visited later */

  scopedvar_delete (target->scopedvar, SV_T_DEPENDENCY, filename);

  mkc_log (target->log, MKC_LOG_TARGET, "get-dep: %s", filename);

  p = str_token (rbuff, dependency_delim, &tokstr);

  while (p != NULL) {
    char    *tp;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    if (first) {
      first = false;
      p = str_token (NULL, dependency_delim, &tokstr);
      continue;
    }

    /* if the compilers supports -MM, then it is much easier to ignore */
    /* system include files */
    if ((flags & TARGET_IGNORE_SYS_INC) == TARGET_IGNORE_SYS_INC &&
      (flags & TARGET_SUPPORTS_MM) != TARGET_SUPPORTS_MM) {
      /* a big assumption */
      if (*p == '/') {
        p = str_token (NULL, dependency_delim, &tokstr);
        continue;
      }
    }

    tp = strrchr (p, '/');
    if (tp == NULL) {
      tp = p;
    } else {
      tp += 1;
    }
    if (strcmp (filename, tp) != 0) {
      mkc_log (target->log, MKC_LOG_TARGET, "  %s", p);
      scopedvar_append_str_list (target->scopedvar, SV_T_DEPENDENCY,
          filename, p, MKC_VCTXT_MKC);
    }
    p = str_token (NULL, dependency_delim, &tokstr);
  }

  free (rbuff);
  chararr_free (cflags);
}

/* creates a list of include files given the paths and match regex */
/* the timestamp argument is set to the latest timestamp */
/* the timestamps in the timestamp namespace are updated */
mkc_list_t *
target_get_include_list (target_t *target, mkc_regex_t *rx,
    int64_t *ts)
{
  value_t         *valhdr = NULL;
  mkc_list_t      *hlist;
#if _have_regex
  mkc_listidx_t   piteridx;
  mkc_listidx_t   pathidx;
  char            *tbuff = NULL;
  char            *tname = NULL;
  char            *tend = NULL;
  char            *path = NULL;
  int64_t         newts = 0;
  char            *p;

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  *tbuff = '\0';

  tname = malloc (MKC_SMALL_BUFF_SZ);
  if (tname == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (tbuff);
    return NULL;
  }
  *tname = '\0';

  path = malloc (MKC_PATH_MAX);
  if (path == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (tbuff);
    free (tname);
    return NULL;
  }
  *path = '\0';

  /* tname is only used internally, there is no need to "clean" the */
  /* variable name, and cleaning it makes it much less specific */
  p = tname;
  tend = tname + MKC_SMALL_BUFF_SZ;
  p = stpecpy (p, tend, "matchts_");
  mkc_list_iter_start (target->attr->pathlist, &piteridx);
  while ((pathidx = mkc_list_iter_next (target->attr->pathlist, &piteridx)) != MKC_ITER_FINISH) {
    value_t   *valpath = NULL;

    valpath = mkc_list_get_by_idx (target->attr->pathlist, pathidx);
    scopedvar_value_get_str (target->scopedvar, valpath, path, MKC_PATH_MAX);
    p = stpecpy (p, tend, path);
    p = stpecpy (p, tend, "_");
  }
  p = stpecpy (p, tend, target->attr->str [MKC_ATTR_MATCH]);

  memcpy (tname, "matchil_", 8);
  if (scopedvar_is_defined (target->scopedvar, SV_T_LOCAL, tname)) {
    valhdr = scopedvar_get_value (target->scopedvar, SV_T_TIMESTAMP, tname);
    hlist = valhdr->list;
    memcpy (tname, "matchts_", 8);
    if (scopedvar_is_defined (target->scopedvar, SV_T_LOCAL, tname)) {
      int64_t   cachedts;

      cachedts = scopedvar_get_timestamp (target->scopedvar, SV_T_LOCAL, tname);
      *ts = cachedts;
      return hlist;
    }
    memcpy (tname, "matchil_", 8);
    scopedvar_delete (target->scopedvar, SV_T_DEPENDENCY, tname);
  }

//  hlist = mkc_list_init (MKC_LIST_UNSORTED, mkc_list_ind_free, NULL, target->mkcerr);
//  hlist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, target->mkcerr);

  mkc_list_iter_start (target->attr->pathlist, &piteridx);
  while ((pathidx = mkc_list_iter_next (target->attr->pathlist, &piteridx)) != MKC_ITER_FINISH) {
    value_t   *valpath = NULL;
    mkc_list_t    *tlist = NULL;
    mkc_listidx_t iteridx;
    mkc_listidx_t idx;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    valpath = mkc_list_get_by_idx (target->attr->pathlist, pathidx);
    scopedvar_value_get_str (target->scopedvar, valpath, path, MKC_PATH_MAX);

    tlist = dir_match (path, rx, target->mkcerr);

    mkc_list_iter_start (tlist, &iteridx);
    while ((idx = mkc_list_iter_next (tlist, &iteridx)) != MKC_ITER_FINISH) {
      char          **temp;
      char          *hdr;
      int64_t       tts;

      temp = mkc_list_get_by_idx (tlist, idx);
      hdr = *temp;

      snprintf (tbuff, MKC_PATH_MAX, "%s/%s", path, hdr);
      tts = fileop_modtime (tbuff);

      /* cache invalidation check */
      if (scopedvar_is_defined (target->scopedvar, SV_T_TIMESTAMP, hdr)) {
        int64_t     cachedfts;

        cachedfts = scopedvar_get_timestamp (target->scopedvar, SV_T_TIMESTAMP, hdr);
        if (tts > cachedfts) {
          scopedvar_delete (target->scopedvar, SV_T_DEPENDENCY, hdr);
        }
      }

      scopedvar_set_timestamp (target->scopedvar, SV_T_TIMESTAMP, hdr, tts, MKC_VCTXT_MKC);
      if (tts > *ts) {
        scopedvar_append_str_list (target->scopedvar, SV_T_DEPENDENCY,
            tname, hdr, MKC_VCTXT_MKC);
//        hdr = strdup (*temp);
//        mkc_list_set (hlist, &hdr, sizeof (char *));
      }
      if (tts > newts) {
        newts = tts;
      }
    }

    mkc_list_free (tlist);
  }

  memcpy (tname, "matchil_", 8);
  valhdr = scopedvar_get_value (target->scopedvar, SV_T_TIMESTAMP, tname);
  hlist = valhdr->list;
  memcpy (tname, "matchts_", 8);
  if (scopedvar_is_defined (target->scopedvar, SV_T_LOCAL, tname)) {
    int64_t   cachedts;

    cachedts = scopedvar_get_timestamp (target->scopedvar, SV_T_LOCAL, tname);
    *ts = cachedts;
  } else {
    /* return the timestamp of the latest include file */
    *ts = newts;
    scopedvar_set_timestamp (target->scopedvar, SV_T_LOCAL, tname, newts, MKC_VCTXT_MKC);
  }

  free (path);
  free (tbuff);
  free (tname);
#endif
  return hlist;
}

const char *
target_iter_includes (target_t *target, mkc_list_t *hlist,
    mkc_listidx_t *hiteridx, char *hdr, size_t hsz)
{
  mkc_listidx_t   hidx;
  char            *retval = NULL;

  while ((hidx = mkc_list_iter_next (hlist, hiteridx)) != MKC_ITER_FINISH) {
    value_t         *tvalue;
    mkc_listidx_t   piteridx;
    mkc_listidx_t   pathidx;
    char            *thdr;

    tvalue = mkc_list_get_by_idx (hlist, hidx);
    scopedvar_value_get_str (target->scopedvar, tvalue, hdr, hsz);
    thdr = strdup (hdr);

    /* always check the current directory */
    if (fileop_exists (thdr)) {
      scopedvar_set_str (target->scopedvar, SV_T_PATHS, thdr, thdr, MKC_VCTXT_MKC);
      retval = thdr;
      return retval;
    }

    mkc_list_iter_start (target->attr->pathlist, &piteridx);
    while ((pathidx = mkc_list_iter_next (target->attr->pathlist, &piteridx)) != MKC_ITER_FINISH) {
      value_t     * valpath;
      const char  * path;

      valpath = mkc_list_get_by_idx (target->attr->pathlist, pathidx);
      path = valpath->sval;

      snprintf (hdr, hsz, "%s/%s", path, thdr);
      if (! fileop_exists (hdr)) {
        continue;
      }

      /* save the path to the header file */
      scopedvar_set_str (target->scopedvar, SV_T_PATHS, thdr, hdr, MKC_VCTXT_MKC);

      retval = hdr + strlen (path) + 1;
      return retval;
    }
  }

  return NULL;
}

void
target_iter_dependency_ts_start (target_t *target, const char *filename,
    mkc_listidx_t *iteridx)
{
  value_t     *value;

  value = scopedvar_get_value (target->scopedvar, SV_T_DEPENDENCY, filename);
  value_iter_start (value, iteridx);
}

int64_t
target_iter_dependency_ts (target_t *target, const char *filename,
    mkc_listidx_t *iteridx)
{
  value_t       *value;
  value_t       tvalue;
  mkc_listidx_t didx;
  int64_t       ts;
  char          dep [MKC_VNAME_MAX];

  value = scopedvar_get_value (target->scopedvar, SV_T_DEPENDENCY, filename);
  didx = value_iter_next (value, &tvalue, iteridx);
  if (didx == MKC_ITER_FINISH) {
    return didx;
  }

  scopedvar_value_get_str (target->scopedvar, &tvalue, dep, sizeof (dep));
  if (! scopedvar_is_defined (target->scopedvar, SV_T_TIMESTAMP, dep)) {
    return 0;
  }

  ts = scopedvar_get_timestamp (target->scopedvar, SV_T_TIMESTAMP, dep);
  return ts;
}

void
target_object_file (target_t *target, const char *execnm,
    const char *objnm)
{
  char    *path;
  int64_t fts = 0;

  path = malloc (MKC_PATH_MAX);
  if (path == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  mkc_log (target->log, MKC_LOG_TARGET, "exec-file: %s\n", execnm);
// ### this is incorrect...but can be fixed later
  path_build (MKC_PATH_MKCF_OBJECTS, path, MKC_PATH_MAX, execnm, target->mkcerr);
  scopedvar_set_str (target->scopedvar, SV_T_PATHS, execnm, path, MKC_VCTXT_MKC);
  fts = fileop_modtime (path);
  scopedvar_set_timestamp (target->scopedvar, SV_T_TIMESTAMP, execnm, fts, MKC_VCTXT_MKC);

  path_build (MKC_PATH_MKCF_OBJECTS, path, MKC_PATH_MAX, objnm, target->mkcerr);
  scopedvar_set_str (target->scopedvar, SV_T_PATHS, objnm, path, MKC_VCTXT_MKC);
  fts = fileop_modtime (path);
  scopedvar_set_timestamp (target->scopedvar, SV_T_TIMESTAMP, objnm, fts, MKC_VCTXT_MKC);

  mkc_log (target->log, MKC_LOG_TARGET, "  %s\n", objnm);
  scopedvar_append_str_list (target->scopedvar, SV_T_DEPENDENCY,
      execnm, objnm, MKC_VCTXT_MKC);

  free (path);
  return;
}

void
target_source_file (target_t *target, const char *objnm,
    const char *srcname)
{
  target_flag_t   tgtflags = TARGET_NONE;
  value_t         tvalue;
  value_t         *valdeplist;
  mkc_listidx_t   diteridx;
  mkc_listidx_t   didx;

  mkc_log (target->log, MKC_LOG_TARGET, "object-file: %s\n", objnm);
  target_get_dependencies (target,
      target->attr->currcompiler, objnm, srcname, tgtflags);

  valdeplist = scopedvar_get_value (target->scopedvar, SV_T_DEPENDENCY, srcname);
  if (valdeplist == NULL) {
    mkc_log (target->log, MKC_LOG_ERROR, "ERR: %s dependency list not found\n", srcname);
    return;
  }

  value_iter_start (valdeplist, &diteridx);
  while ((didx = value_iter_next (valdeplist, &tvalue, &diteridx)) != MKC_ITER_FINISH) {
    char    dep [MKC_VNAME_MAX];
    char    *p;

    scopedvar_value_get_str (target->scopedvar, &tvalue, dep, sizeof (dep));
    mkc_log (target->log, MKC_LOG_TARGET, "  %s\n", dep);
    p = strrchr (dep, '/');
    if (p == NULL) {
      p = dep;
    } else {
      p += 1;
    }

    scopedvar_append_str_list (target->scopedvar, SV_T_DEPENDENCY,
        objnm, dep, MKC_VCTXT_MKC);
  }

  return;
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
