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
#include "compile.h"
#include "dirmatch.h"
#include "dirop.h"
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
#include "toposort.h"
#include "value.h"

typedef struct target_t {
  scopedvar_t         * scopedvar;
  compile_t           * compile;
  mkc_attribute_t     * attr;
  mkc_log_t           * log;
  mkc_error_t         * mkcerr;
  bool                create_stage_bin;
} target_t;

static const char * const dependency_delim = " \n\r\\";

static bool target_chk_last_libloc (mkc_compiler_id_t compid, char *lastlibloc, size_t sz, const char *str);
static void target_process_timestamp (target_t *target, char *path, size_t psz, const char *filename);
static void target_topo_add_items_deps (target_t *target, toposort_t *topo, mkc_list_t *itemlist);
static void target_create_stage_bin (target_t *target);

target_t *
target_init (scopedvar_t *scopedvar, compile_t *compile,
    mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr)
{
  target_t    *target;

  target = malloc (sizeof (target_t));
  if (target == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  target->scopedvar = scopedvar;
  target->compile = compile;
  target->attr = attr;
  target->log = log;
  target->mkcerr = mkcerr;
  target->create_stage_bin = false;

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

MKC_NODISCARD
chararr_t *
target_get_flags (target_t *target, const char *flagname,
    chararr_t *include_paths)
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
  chararr_set_freeinternals (flags);

  sviter = scopedvar_iter_start (scopedvar, SV_ITER_HIERARCHY);
  while ((profnm = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    value_t         *value = NULL;
    mkc_listidx_t   fiter;
    mkc_listidx_t   fidx;
    sv_type_t       svtype;
    bool            append_next = false;

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
      if (target_chk_last_libloc (target->attr->compid,
          lastlibloc, MKC_PATH_MAX, str)) {
        /* de-duplication check */
        continue;
      }

      if (include_paths != NULL) {
        size_t      len;

        if (append_next) {
          chararr_append (include_paths, strdup (str));
          append_next = false;
        }
        len = compiler_get_flag_len (target->attr->compid, MKC_COMP_FLAG_INCLUDE);
        if (strncmp (str,
            compiler_get_flag (target->attr->compid, MKC_COMP_FLAG_INCLUDE),
            len) == 0) {
          if (strcmp (str,
              compiler_get_flag (target->attr->compid, MKC_COMP_FLAG_INCLUDE)) == 0) {
            append_next = true;
          } else {
            chararr_append (include_paths, strdup (str + len));
          }
        }
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
target_topo_add_items (target_t *target, toposort_t *topo, mkc_list_t *itemlist)
{
  mkc_listidx_t   hiteridx;
  mkc_listidx_t   hidx;

  mkc_list_iter_start (itemlist, &hiteridx);
  while ((hidx = mkc_list_iter_next (itemlist, &hiteridx)) != MKC_ITER_FINISH) {
    char        **temp;
    const char  *item;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    temp = mkc_list_get_by_idx (itemlist, hidx);
    item = *temp;
    toposort_add_item (topo, item);
  }
}

void
target_topo_add_deps (target_t *target,
    toposort_t *topo, const char *tgtname)
{
  value_t         * valdeplist;
  mkc_listidx_t   diteridx;
  mkc_listidx_t   didx;
  value_t         tvalue;
  char            * dep;

  mkc_log (target->log, MKC_LOG_CHECK, "add-dep %s :\n", tgtname);
  valdeplist = scopedvar_get_value (target->scopedvar, SV_T_DEPENDENCY, tgtname);
  if (valdeplist == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_FATAL_ERROR, 0, tgtname);
    fprintf (stderr, "ERR: unable to locate dep list for %s\n", tgtname);
    return;
  }

  dep = malloc (MKC_PATH_MAX);
  if (dep == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  value_iter_start (valdeplist, &diteridx);
  while ((didx = value_iter_next (valdeplist, &tvalue, &diteridx)) != MKC_ITER_FINISH) {
    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    scopedvar_value_get_str (target->scopedvar, &tvalue, dep, MKC_PATH_MAX);
    mkc_log (target->log, MKC_LOG_CHECK, "  %s\n", dep);
    toposort_add_pair (topo, tgtname, dep);
  }

  free (dep);
}

int
target_check_dependency_timestamp (target_t *target,
    const char *filename, const char *filepath)
{
  int64_t       fts;
  int64_t       ts;
  mkc_listidx_t iteridx;

  mkc_message (MKC_V_TMI, "   chk-dep-ts: %s ", filepath);

  if (! scopedvar_is_defined (target->scopedvar, SV_T_DEPENDENCY, filepath)) {
fprintf (stderr, "no dep entry, ood\n");
    return TARGET_OUT_OF_DATE;
  }

  fts = 0;
  if (scopedvar_is_defined (target->scopedvar, SV_T_TIMESTAMP, filepath)) {
    fts = scopedvar_get_timestamp (target->scopedvar, SV_T_TIMESTAMP, filepath);
  }
fprintf (stderr, "fts: %zd ", fts);

  target_iter_dependency_ts_start (target, filepath, &iteridx);
  while ((ts = target_iter_dependency_ts (target, filepath, &iteridx)) != MKC_ITER_FINISH) {
    if (ts > fts) {
fprintf (stderr, "ood\n");
      return TARGET_OUT_OF_DATE;
    }
  }

fprintf (stderr, "curr\n");
  return TARGET_CURRENT;
}

void
target_get_dependencies (target_t *target,
    mkc_compiler_t compiler, const char *tgtname, const char *filepath,
    target_flag_t flags, chararr_t *cflags)
{
  int             rc;
  char            * rbuff;
  size_t          rsz;
  char            * tokstr;
  mkc_list_t      * elist;
  value_t         evalue;
  char            * p;
  bool            first = true;

  mkc_message (MKC_V_TMI, "   get-deps: %s\n", filepath);

  rsz = MKC_LARGE_BUFF_SZ;
  rbuff = malloc (rsz);
  if (rbuff == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  *rbuff = '\0';

  if ((flags & TARGET_USE_MM) == TARGET_USE_MM) {
    compile_append_compflag (target->compile,
        compiler_get_flag (target->attr->compid, MKC_COMP_FLAG_DEPS_USER));
  } else {
    compile_append_compflag (target->compile,
        compiler_get_flag (target->attr->compid, MKC_COMP_FLAG_DEPS));
  }
  compile_append_compflag (target->compile, NULL);
  compile_preprocess (target->compile);
  compile_set_flags (target->compile, cflags, NULL, NULL);
  target->attr->printerrors = true;
  rc = compile_exec (target->compile, COMPILE_COMPILE, compiler,
      filepath, rbuff, rsz);
  compile_reset (target->compile);
  if (rc != MKC_OK) {
    free (rbuff);
    return;
  }

  scopedvar_delete (target->scopedvar, SV_T_DEPENDENCY, tgtname);

  /* the dependency list must exist */
  elist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, target->mkcerr);
  evalue.vtype = MKC_VT_LIST;
  evalue.list = elist;
  scopedvar_set (target->scopedvar, SV_T_DEPENDENCY, tgtname, &evalue, MKC_VCTXT_MKC);
  mkc_list_free (elist);

  mkc_log (target->log, MKC_LOG_TARGET, "get-dep: %s\n", filepath);

  p = str_token (rbuff, dependency_delim, &tokstr);

  while (p != NULL) {
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

    if (strcmp (tgtname, p) != 0) {
      /* do not add self as a dependency */
      mkc_log (target->log, MKC_LOG_TARGET, "  %s\n", p);
      scopedvar_append_str_list (target->scopedvar, SV_T_DEPENDENCY,
          tgtname, p, MKC_VCTXT_MKC);
    }
    p = str_token (NULL, dependency_delim, &tokstr);
  }

  free (rbuff);
}

/* creates a list of include files given the paths and match regex */
/* the timestamp argument is set to the latest timestamp */
/* the timestamps in the timestamp namespace are updated */
/* used for check_include_dependencies, check_include_guards, */
/*   check_include_compile */
mkc_list_t *
target_get_include_list (target_t *target, chararr_t * include_paths,
    mkc_regex_t *rx, int64_t *ts)
{
  value_t         * valhdr = NULL;
  mkc_list_t      * hlist = NULL;
#if _have_regex
  char            * hdrpath = NULL;
  char            * tname = NULL;
  char            * tend = NULL;
  scopedvar_t     * scopedvar = NULL;
  const char      ** patharr;
  const char      * path = NULL;
  int64_t         newts = 0;
  char            * p;
  int             count;

  tname = malloc (MKC_SMALL_BUFF_SZ);
  if (tname == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  *tname = '\0';

  /* messy stuff to cache the include timestamps and include list */
  /* this cache is used for the check_include_* statements */

  /* tname is only used internally, there is no need to "clean" the */
  /* variable name, and cleaning it makes it much less specific */
  p = tname;
  tend = tname + MKC_SMALL_BUFF_SZ;
  p = stpecpy (p, tend, "matchts_");

  patharr = chararr_get_arr (include_paths);
  count = 0;
  while ((path = patharr [count++]) != NULL) {
    p = stpecpy (p, tend, path);
    p = stpecpy (p, tend, "_");
  }
  p = stpecpy (p, tend, target->attr->str [MKC_ATTR_MATCH]);

  scopedvar = target->scopedvar;

  memcpy (tname, "matchil_", 8);
  if (scopedvar_is_defined (scopedvar, SV_T_LOCAL, tname)) {
    valhdr = scopedvar_get_value (scopedvar, SV_T_LOCAL, tname);
    hlist = valhdr->list;
    memcpy (tname, "matchts_", 8);
    if (scopedvar_is_defined (scopedvar, SV_T_LOCAL, tname)) {
      int64_t   cachedts;

      cachedts = scopedvar_get_timestamp (scopedvar, SV_T_LOCAL, tname);
      *ts = cachedts;
      free (tname);
      return hlist;
    }
    memcpy (tname, "matchil_", 8);
    scopedvar_delete (scopedvar, SV_T_LOCAL, tname);
  }

  hdrpath = malloc (MKC_PATH_MAX);
  if (hdrpath == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (tname);
    return NULL;
  }
  *hdrpath = '\0';

  count = 0;
  while ((path = patharr [count++]) != NULL) {
    mkc_list_t    *tlist = NULL;
    mkc_listidx_t iteridx;
    mkc_listidx_t idx;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    /* all of the header files for this path */
    tlist = dir_match (path, rx, target->mkcerr);

    mkc_list_iter_start (tlist, &iteridx);
    while ((idx = mkc_list_iter_next (tlist, &iteridx)) != MKC_ITER_FINISH) {
      char          **temp;
      char          *hdr;
      int64_t       tts;

      temp = mkc_list_get_by_idx (tlist, idx);
      hdr = *temp;

      if (strcmp (path, ".") == 0) {
        stpecpy (hdrpath, hdrpath + MKC_PATH_MAX, hdr);
      } else {
        snprintf (hdrpath, MKC_PATH_MAX, "%s/%s", path, hdr);
      }
      scopedvar_set_str (scopedvar, SV_T_PATHS, hdr, hdrpath, MKC_VCTXT_MKC);
      tts = fileop_modtime (hdrpath);

      /* cache invalidation check */
      /* if the timestamp of the file is newer than the cached timestamp */
      /* for the header, then the list of dependencies for the header */
      /* is out of date and must be cleared */
      /* header dependencies are used for the 'check_include_...' tests */
      if (scopedvar_is_defined (scopedvar, SV_T_TIMESTAMP, hdrpath)) {
        int64_t     cachedts;

        cachedts = scopedvar_get_timestamp (scopedvar, SV_T_TIMESTAMP, hdrpath);
        if (tts > cachedts) {
          scopedvar_delete (scopedvar, SV_T_DEPENDENCY, hdrpath);
        }
      }

      scopedvar_set_timestamp (scopedvar, SV_T_TIMESTAMP, hdrpath, tts, MKC_VCTXT_MKC);
      scopedvar_set_integer (scopedvar, SV_T_BUILD, hdrpath, TGT_T_INCLUDE, MKC_VCTXT_MKC);
      if (tts > *ts) {
        scopedvar_append_str_list (scopedvar, SV_T_LOCAL,
            tname, hdrpath, MKC_VCTXT_MKC);
      }
      if (tts > newts) {
        newts = tts;
      }
    }

    mkc_list_free (tlist);
  }

  memcpy (tname, "matchil_", 8);
  valhdr = scopedvar_get_value (scopedvar, SV_T_LOCAL, tname);
  hlist = valhdr->list;

  memcpy (tname, "matchts_", 8);
  /* return the timestamp of the latest include file */
  *ts = newts;
  scopedvar_set_timestamp (scopedvar, SV_T_LOCAL, tname, newts, MKC_VCTXT_MKC);

  free (hdrpath);
  free (tname);
#endif
  return hlist;
}

/* used for check_include_dependencies, check_include_guards, */
/*   check_include_compile */
const char *
target_iter_includes (target_t *target, mkc_list_t *hlist,
    mkc_listidx_t *hiteridx, char *hdrpath, size_t hpsz)
{
  mkc_listidx_t   hidx;

  while ((hidx = mkc_list_iter_next (hlist, hiteridx)) != MKC_ITER_FINISH) {
    value_t     *tvalue;
    const char  *p;

    tvalue = mkc_list_get_by_idx (hlist, hidx);
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
  char        *epath;
  char        *opath;
  int64_t     fts = 0;
  scopedvar_t *scopedvar = target->scopedvar;
  value_t     * value;
  bool        changed;

  value = scopedvar_get_value (target->scopedvar, SV_T_INTERNAL,
      MKC_C_MKC_CHANGED);
  changed = scopedvar_value_get_integer (target->scopedvar, value);

  epath = malloc (MKC_PATH_MAX);
  if (epath == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  opath = malloc (MKC_PATH_MAX);
  if (opath == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (epath);
    return;
  }

  mkc_log (target->log, MKC_LOG_TARGET, "exec-file: %s %s\n", execnm, objnm);

  path_build (MKC_PATH_STAGE_BIN, epath, MKC_PATH_MAX, execnm, target->mkcerr);
  scopedvar_set_str (scopedvar, SV_T_PATHS, execnm, epath, MKC_VCTXT_MKC);
  fts = fileop_modtime (epath);
  scopedvar_set_timestamp (scopedvar, SV_T_TIMESTAMP, epath, fts, MKC_VCTXT_MKC);
  scopedvar_set_integer (scopedvar, SV_T_BUILD, epath, TGT_T_EXEC, MKC_VCTXT_MKC);

  path_build (MKC_PATH_MKCF_OBJECTS, opath, MKC_PATH_MAX, objnm, target->mkcerr);
  scopedvar_set_str (scopedvar, SV_T_PATHS, objnm, opath, MKC_VCTXT_MKC);
  fts = fileop_modtime (opath);
fprintf (stderr, "   e-obj: set ts %s %zd\n", opath, fts);
  scopedvar_set_timestamp (scopedvar, SV_T_TIMESTAMP, opath, fts, MKC_VCTXT_MKC);
  scopedvar_set_integer (scopedvar, SV_T_BUILD, opath, TGT_T_OBJECT, MKC_VCTXT_MKC);

  mkc_log (target->log, MKC_LOG_TARGET, "  %s\n", objnm);
  if (changed) {
    scopedvar_append_str_list (scopedvar, SV_T_DEPENDENCY,
        epath, opath, MKC_VCTXT_MKC);
  }

  free (epath);
  free (opath);
  return;
}

void
target_object_source (target_t *target, const char *objnm,
    const char *srcname)
{
  target_flag_t   tgtflags = TARGET_NONE;
  value_t         * value;
  value_t         tvalue;
  value_t         * valdeplist;
  mkc_listidx_t   diteridx;
  mkc_listidx_t   didx;
  char            * path;
  char            * opath;
  chararr_t       * cflags;

  opath = malloc (MKC_PATH_MAX);
  if (opath == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  value = scopedvar_get_value (target->scopedvar, SV_T_PATHS, objnm);
  scopedvar_value_get_str (target->scopedvar, value, opath, MKC_PATH_MAX);

  mkc_log (target->log, MKC_LOG_TARGET, "object-file: %s %s\n", objnm, srcname);

  if (target_check_dependency_timestamp (
      target, opath, srcname) == TARGET_OUT_OF_DATE) {
    mkc_message (MKC_V_INFO, "-- getting dependencies for %s\n", objnm);
    cflags = target_get_flags (target, MKC_C_CFLAGS, NULL);
    target_get_dependencies (target,
        target->attr->currcompiler, opath, srcname, tgtflags, cflags);
    chararr_free (cflags);
  }

  valdeplist = scopedvar_get_value (target->scopedvar, SV_T_DEPENDENCY, opath);
  if (valdeplist == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_FATAL_ERROR, 0, NULL);
    mkc_log (target->log, MKC_LOG_ERROR, "ERR: %s dependency list not found\n", opath);
    free (opath);
    return;
  }

  path = malloc (MKC_PATH_MAX);
  if (path == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (opath);
    return;
  }

  scopedvar_set_integer (target->scopedvar, SV_T_BUILD, srcname, TGT_T_SOURCE, MKC_VCTXT_MKC);

  value_iter_start (valdeplist, &diteridx);
  while ((didx = value_iter_next (valdeplist, &tvalue, &diteridx)) != MKC_ITER_FINISH) {
    char        dep [MKC_VNAME_MAX];

    scopedvar_value_get_str (target->scopedvar, &tvalue, dep, sizeof (dep));
    target_process_timestamp (target, path, MKC_PATH_MAX, dep);
  }

  free (path);
  free (opath);
  return;
}

void
target_build (target_t *target, mkc_list_t *blist)
{
  toposort_t      * topo;
  const char      * builditem;
  char            * dep;
  char            * source;
  int             rc;
  chararr_t       * cflags;
  chararr_t       * ldflags;
  chararr_t       * libs;

  topo = toposort_init (target->mkcerr);

  target_topo_add_items_deps (target, topo, blist);
  if (mkc_error_chk_err (target->mkcerr)) {
    return;
  }

  rc = toposort (topo);
  if (rc == MKC_ERR_FAILURE) {
    toposort_free (topo);
    return;
  }

  dep = malloc (MKC_PATH_MAX);
  if (dep == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    toposort_free (topo);
    return;
  }

  source = malloc (MKC_PATH_MAX);
  if (source == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (dep);
    toposort_free (topo);
    return;
  }

  cflags = target_get_flags (target, MKC_C_CFLAGS, NULL);
  ldflags = target_get_flags (target, MKC_C_LDFLAGS, NULL);
  libs = target_get_flags (target, MKC_C_LIBS, NULL);

  toposort_iter_start (topo);
  while ((builditem = toposort_iter_next_reverse (topo)) != NULL) {
    value_t         *value;
    value_t         * valdeplist;
    value_t         tvalue;
    mkc_listidx_t   diteridx;
    mkc_listidx_t   didx;
    int             tgttype;
    ct_type_t       comptype = COMPILE_COMPILE;
    const char      *buildtag = "";

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    value = scopedvar_get_value (target->scopedvar, SV_T_BUILD, builditem);
    if (value == NULL) {
      continue;
    }

    tgttype = scopedvar_value_get_integer (target->scopedvar, value);
    switch (tgttype) {
      case TGT_T_EXEC: {
        target_create_stage_bin (target);
        compile_set_output (target->compile, builditem);
        comptype = COMPILE_LINK;
        buildtag = "link";
        break;
      }
      case TGT_T_INCLUDE: {
        continue;
      }
      case TGT_T_FILE: {
        buildtag = "create";
        break;
      }
      case TGT_T_OBJECT: {
        compile_set_output (target->compile, builditem);
        buildtag = "compile";
        break;
      }
      case TGT_T_SOURCE: {
        break;
      }
    }

    if (tgttype != TGT_T_SOURCE) {
      if (target_check_dependency_timestamp (
          target, builditem, builditem) == TARGET_CURRENT) {
        continue;
      }

      mkc_log (target->log, MKC_LOG_GENERAL, "== %s: %s\n", buildtag, builditem);
      mkc_message (MKC_V_BASIC, "-- %s: %s\n", buildtag, builditem);
    }

    valdeplist = scopedvar_get_value (target->scopedvar, SV_T_DEPENDENCY, builditem);
    value_iter_start (valdeplist, &diteridx);
    while ((didx = value_iter_next (valdeplist, &tvalue, &diteridx)) != MKC_ITER_FINISH) {
      int       ttgttype;

      if (mkc_error_chk_err (target->mkcerr)) {
        break;
      }

      scopedvar_value_get_str (target->scopedvar, &tvalue, dep, MKC_PATH_MAX);
      value = scopedvar_get_value (target->scopedvar, SV_T_BUILD, dep);

      if (value == NULL) {
        continue;
      }

      ttgttype = scopedvar_value_get_integer (target->scopedvar, value);
      if (tgttype == TGT_T_EXEC && ttgttype == TGT_T_OBJECT) {
        char    *tmp;

        tmp = strdup (dep);
        compile_append_object (target->compile, tmp);
      }
      if (tgttype == TGT_T_OBJECT && ttgttype == TGT_T_SOURCE) {
        stpecpy (source, source + MKC_PATH_MAX, dep);
      }
    }

    if (tgttype == TGT_T_EXEC || tgttype == TGT_T_OBJECT) {
      int64_t   tts;
      int       rc;

      compile_set_flags (target->compile, cflags, ldflags, libs);
      target->attr->printerrors = true;
      rc = compile_exec (target->compile, comptype, target->attr->currcompiler,
          source, NULL, 0);
      compile_reset (target->compile);
      if (rc != MKC_OK) {
        continue;
      }

      tts = fileop_modtime (builditem);
fprintf (stderr, "   build: set ts %s\n", builditem);
      scopedvar_set_timestamp (target->scopedvar, SV_T_TIMESTAMP, builditem, tts, MKC_VCTXT_MKC);
    }
  }

  chararr_free (cflags);
  chararr_free (ldflags);
  chararr_free (libs);

  toposort_free (topo);
  free (dep);
  free (source);
  return;
}

/* internal routines */

static bool
target_chk_last_libloc (mkc_compiler_id_t compid,
    char *lastlibloc, size_t sz, const char *str)
{
  if (! mkc_flag_is_libloc (compid, str)) {
    return false;
  }

  if (strcmp (lastlibloc, str) == 0) {
    return true;
  }

  stpecpy (lastlibloc, lastlibloc + sz, str);
  return false;
}

static void
target_process_timestamp (target_t *target,
    char *path, size_t psz, const char *filename)
{
  value_t     *value;
  int64_t     ts = 0;

  if (scopedvar_is_defined (target->scopedvar, SV_T_TIMESTAMP, filename)) {
    return;
  }

  stpecpy (path, path + psz, filename);
  if (*path != '/') {
    value = scopedvar_get_value (target->scopedvar, SV_T_PATHS, filename);
    if (value == NULL) {
// ### need the set of paths from cflags
    } else {
      scopedvar_value_get_str (target->scopedvar, value, path, psz);
    }
  }

  ts = fileop_modtime (path);
fprintf (stderr, "   proc-ts: set ts %s\n", filename);
  scopedvar_set_timestamp (target->scopedvar, SV_T_TIMESTAMP, filename, ts, MKC_VCTXT_MKC);
}

static void
target_topo_add_items_deps (target_t *target, toposort_t *topo,
    mkc_list_t *itemlist)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   idx;
  char            * itemnm;
  mkc_list_t      * ilist;

  itemnm = malloc (MKC_PATH_MAX);
  if (itemnm == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  ilist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, target->mkcerr);

  mkc_list_iter_start (itemlist, &iteridx);
  while ((idx = mkc_list_iter_next (itemlist, &iteridx)) != MKC_ITER_FINISH) {
    value_t   * value;
    value_t   * tvalue;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    value = mkc_list_get_by_idx (itemlist, idx);
    scopedvar_value_get_str (target->scopedvar, value, itemnm, MKC_PATH_MAX);

    tvalue = scopedvar_get_value (target->scopedvar, SV_T_PATHS, itemnm);
    if (tvalue == NULL) {
      tvalue = value;
    }
    mkc_list_set (ilist, tvalue, sizeof (value_t));
  }
  target_topo_add_items (target, topo, ilist);

  mkc_list_iter_start (ilist, &iteridx);
  while ((idx = mkc_list_iter_next (ilist, &iteridx)) != MKC_ITER_FINISH) {
    value_t     * value;
    value_t     * valdeplist;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    value = mkc_list_get_by_idx (ilist, idx);
    scopedvar_value_get_str (target->scopedvar, value, itemnm, MKC_PATH_MAX);

    valdeplist = scopedvar_get_value (target->scopedvar, SV_T_DEPENDENCY, itemnm);
    if (valdeplist == NULL) {
      /* items without dependency lists are include files */
      continue;
    }

    value = scopedvar_get_value (target->scopedvar, SV_T_BUILD, itemnm);
    if (value != NULL) {
      int             tgttype;

      tgttype = scopedvar_value_get_integer (target->scopedvar, value);
      if (tgttype == TGT_T_INCLUDE) {
        continue;
      }
    }

    target_topo_add_items_deps (target, topo, valdeplist->list);

    if (! scopedvar_is_defined (target->scopedvar, SV_T_DEPENDENCY, itemnm)) {
      continue;
    }

    target_topo_add_deps (target, topo, itemnm);
  }

  mkc_list_free (ilist);
  free (itemnm);
}

static void
target_create_stage_bin (target_t *target)
{
  char    *epath;

  if (target->create_stage_bin) {
    return;
  }

  epath = malloc (MKC_PATH_MAX);
  if (epath == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  path_build (MKC_PATH_STAGE_BIN, epath, MKC_PATH_MAX, NULL, target->mkcerr);
  dirop_make (epath, target->mkcerr);
  free (epath);
  target->create_stage_bin = true;
}
