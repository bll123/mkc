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
static void target_process_timestamp (target_t *target, const char *filename);

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
  mkc_list_t      * elist;
  value_t         evalue;
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
  if ((flags & TARGET_USE_MM) == TARGET_USE_MM) {
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

  scopedvar_delete (target->scopedvar, SV_T_DEPENDENCY, filename);

  /* the dependency list must exist */
  elist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, target->mkcerr);
  evalue.vtype = MKC_VT_LIST;
  evalue.list = elist;
  scopedvar_set (target->scopedvar, SV_T_DEPENDENCY, filename, &evalue, MKC_VCTXT_MKC);
  mkc_list_free (elist);

  mkc_log (target->log, MKC_LOG_TARGET, "get-dep: %s", filename);

  p = str_token (rbuff, dependency_delim, &tokstr);

  while (p != NULL) {
    const char  *tfn;

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
      (flags & TARGET_USE_MM) != TARGET_USE_MM) {
      /* a big assumption */
      if (*p == '/') {
        p = str_token (NULL, dependency_delim, &tokstr);
        continue;
      }
    }

    tfn = path_filename (p);
    if (strcmp (filename, tfn) != 0) {
      mkc_log (target->log, MKC_LOG_TARGET, "  %s", p);
      scopedvar_append_str_list (target->scopedvar, SV_T_DEPENDENCY,
          filename, p, MKC_VCTXT_MKC);
    }
fprintf (stderr, "gd: %s %s\n", tfn, p);
    scopedvar_set_str (target->scopedvar, SV_T_PATHS, tfn, p, MKC_VCTXT_MKC);
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

  tname = malloc (MKC_SMALL_BUFF_SZ);
  if (tname == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  *tname = '\0';

  path = malloc (MKC_PATH_MAX);
  if (path == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
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
    valhdr = scopedvar_get_value (target->scopedvar, SV_T_LOCAL, tname);
    hlist = valhdr->list;
    memcpy (tname, "matchts_", 8);
    if (scopedvar_is_defined (target->scopedvar, SV_T_LOCAL, tname)) {
      int64_t   cachedts;

      cachedts = scopedvar_get_timestamp (target->scopedvar, SV_T_LOCAL, tname);
      *ts = cachedts;
      free (path);
      free (tname);
      return hlist;
    }
    memcpy (tname, "matchil_", 8);
    scopedvar_delete (target->scopedvar, SV_T_LOCAL, tname);
  }

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  *tbuff = '\0';

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

    /* all of the header files for this path */
    tlist = dir_match (path, rx, target->mkcerr);

    mkc_list_iter_start (tlist, &iteridx);
    while ((idx = mkc_list_iter_next (tlist, &iteridx)) != MKC_ITER_FINISH) {
      char          **temp;
      char          *hdr;
      int64_t       tts;

      temp = mkc_list_get_by_idx (tlist, idx);
      hdr = *temp;

      snprintf (tbuff, MKC_PATH_MAX, "%s/%s", path, hdr);
      scopedvar_set_str (target->scopedvar, SV_T_PATHS, hdr, tbuff, MKC_VCTXT_MKC);
      tts = fileop_modtime (tbuff);

      /* cache invalidation check */
      /* if the timestamp of the file is newer than the cached timestamp */
      /* for the header, then the list of dependencies for the header */
      /* is out of date and must be cleared */
      /* header dependencies are used for the 'check_include_...' tests */
      if (scopedvar_is_defined (target->scopedvar, SV_T_TIMESTAMP, hdr)) {
        int64_t     cachedts;

        cachedts = scopedvar_get_timestamp (target->scopedvar, SV_T_TIMESTAMP, hdr);
        if (tts > cachedts) {
          scopedvar_delete (target->scopedvar, SV_T_DEPENDENCY, hdr);
        }
      }

      scopedvar_set_timestamp (target->scopedvar, SV_T_TIMESTAMP, hdr, tts, MKC_VCTXT_MKC);
      if (tts > *ts) {
        scopedvar_append_str_list (target->scopedvar, SV_T_LOCAL,
            tname, hdr, MKC_VCTXT_MKC);
      }
      if (tts > newts) {
        newts = tts;
      }
    }

    mkc_list_free (tlist);
  }

  memcpy (tname, "matchil_", 8);
  valhdr = scopedvar_get_value (target->scopedvar, SV_T_LOCAL, tname);
  hlist = valhdr->list;

  memcpy (tname, "matchts_", 8);
  /* return the timestamp of the latest include file */
  *ts = newts;
  scopedvar_set_timestamp (target->scopedvar, SV_T_LOCAL, tname, newts, MKC_VCTXT_MKC);

  free (path);
  free (tbuff);
  free (tname);
#endif
  return hlist;
}

const char *
target_iter_includes (target_t *target, mkc_list_t *hlist,
    mkc_listidx_t *hiteridx, char *hdrpath, size_t hpsz)
{
  mkc_listidx_t   hidx;

  while ((hidx = mkc_list_iter_next (hlist, hiteridx)) != MKC_ITER_FINISH) {
    value_t     *tvalue;
    char        hdr [MKC_VNAME_MAX];
    const char  *p;

    tvalue = mkc_list_get_by_idx (hlist, hidx);
    scopedvar_value_get_str (target->scopedvar, tvalue, hdr, sizeof (hdr));
    tvalue = scopedvar_get_value (target->scopedvar, SV_T_PATHS, hdr);
    scopedvar_value_get_str (target->scopedvar, tvalue, hdrpath, hpsz);
    p = path_filename (hdrpath);
    return p;
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
target_executable_object (target_t *target, const char *execnm,
    const char *objnm)
{
  char    *path;
  int64_t fts = 0;

fprintf (stderr, "exec-obj: %s %s\n", execnm, objnm);
  path = malloc (MKC_PATH_MAX);
  if (path == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  mkc_log (target->log, MKC_LOG_TARGET, "exec-file: %s %s\n", execnm, objnm);
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
target_object_source (target_t *target, const char *objnm,
    const char *srcname)
{
  target_flag_t   tgtflags = TARGET_NONE;
  value_t         tvalue;
  value_t         *valdeplist;
  mkc_listidx_t   diteridx;
  mkc_listidx_t   didx;

fprintf (stderr, "object-file: %s %s\n", objnm, srcname);
  mkc_log (target->log, MKC_LOG_TARGET, "object-file: %s %s\n", objnm, srcname);
  target_get_dependencies (target,
      target->attr->currcompiler, objnm, srcname, tgtflags);

  valdeplist = scopedvar_get_value (target->scopedvar, SV_T_DEPENDENCY, objnm);
  if (valdeplist == NULL) {
    mkc_log (target->log, MKC_LOG_ERROR, "ERR: %s dependency list not found\n", srcname);
    return;
  }

  value_iter_start (valdeplist, &diteridx);
  while ((didx = value_iter_next (valdeplist, &tvalue, &diteridx)) != MKC_ITER_FINISH) {
    char        dep [MKC_VNAME_MAX];
    const char  *p;

    scopedvar_value_get_str (target->scopedvar, &tvalue, dep, sizeof (dep));
    mkc_log (target->log, MKC_LOG_TARGET, "  %s\n", dep);
    p = path_filename (dep);

fprintf (stderr, "     obj-src: dep %s %s\n", dep, p);
    scopedvar_set_str (target->scopedvar, SV_T_PATHS, p, dep, MKC_VCTXT_MKC);
    scopedvar_append_str_list (target->scopedvar, SV_T_DEPENDENCY,
        objnm, dep, MKC_VCTXT_MKC);
    target_process_timestamp (target, p);
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

static void
target_process_timestamp (target_t *target, const char *filename)
{
  value_t     *value;
  int64_t     ts = 0;
  char        *path;

  if (scopedvar_is_defined (target->scopedvar, SV_T_TIMESTAMP, filename)) {
    return;
  }

  path = malloc (MKC_PATH_MAX);
  if (path == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  stpecpy (path, path + MKC_PATH_MAX, filename);
  if (*path != '/') {
    value = scopedvar_get_value (target->scopedvar, SV_T_PATHS, filename);
    if (value == NULL) {
// ### need the set of paths from cflags
    } else {
      scopedvar_value_get_str (target->scopedvar, value, path, MKC_PATH_MAX);
    }
  }

  ts = fileop_modtime (path);
  scopedvar_set_timestamp (target->scopedvar, SV_T_TIMESTAMP, filename, ts, MKC_VCTXT_MKC);
  free (path);
}

