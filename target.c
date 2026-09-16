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
#include "const.h"
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
  scopedvar_t         * sv;
  compile_t           * compile;
  mkc_attribute_t     * attr;
  mkc_log_t           * log;
  mkc_error_t         * mkcerr;
  bool                create_stage_bin;
} target_t;

static const char * const dependency_delim = " \n\r\\";

static bool target_chk_last_libloc (mkc_compiler_id_t compid, char *lastlibloc, size_t sz, const char *str);
static void target_process_timestamp (target_t *target, char *path, size_t psz, const char *filename);
static void target_topo_add_items_deps (target_t *target, toposort_t *topo, list_t *itemlist);
static void target_create_stage_bin (target_t *target);

target_t *
target_init (scopedvar_t *sv, compile_t *compile,
    mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr)
{
  target_t    *target;

  target = malloc (sizeof (target_t));
  if (target == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  target->sv = sv;
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
  char            * lastlibloc;
  char            * str;
  scopedvar_t     * sv;
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

  sv = target->sv;

  flags = chararr_init (target->mkcerr);
  if (flags == NULL) {
    return NULL;
  }
  chararr_set_freeinternals (flags);

  sviter = sv_iter_start (sv, SV_ITER_HIERARCHY);
  while ((profnm = sv_iter_next (sv, sviter)) != NULL) {
    value_t     * value = NULL;
    listidx_t   fiter;
    listidx_t   fidx;
    sv_type_t   svtype;
    bool        append_next = false;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    svtype = sv_iter_get_type (sv, sviter);
    value = sv_get_value (sv, svtype, flagname, NULL);
    if (value == NULL || value->vtype != MKC_VT_LIST) {
      continue;
    }

    list_iter_start (value->list, &fiter);
    while ((fidx = list_iter_next (value->list, &fiter)) != MKC_ITER_FINISH) {
      value_t   *fval;

      if (mkc_error_chk_err (target->mkcerr)) {
        break;
      }

      fval = list_get_by_idx (value->list, fidx);
      sv_value_get_str (sv, fval, str, MKC_PATH_MAX);
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
  sv_iter_finish (sviter);
  chararr_append (flags, NULL);

  free (lastlibloc);
  free (str);

  return flags;
}

void
target_topo_add_items (target_t *target, toposort_t *topo, list_t *itemlist)
{
  listidx_t   hiteridx;
  listidx_t   hidx;

  list_iter_start (itemlist, &hiteridx);
  while ((hidx = list_iter_next (itemlist, &hiteridx)) != MKC_ITER_FINISH) {
    char        **temp;
    const char  *item;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    temp = list_get_by_idx (itemlist, hidx);
    item = *temp;
    toposort_add_item (topo, item);
  }
}

void
target_topo_add_deps (target_t *target,
    toposort_t *topo, const char *tgtname)
{
  value_t         * valdeplist;
  listidx_t   diteridx;
  listidx_t   didx;
  value_t         tvalue;
  char            * dep;

  mkc_log (target->log, MKC_LOG_CHECK, "add-dep %s :\n", tgtname);
  valdeplist = sv_get_value (target->sv, SV_T_BUILD_DATA,
      tgtname, MKC_C_BVAR_DEPENDENCY);
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

    sv_value_get_str (target->sv, &tvalue, dep, MKC_PATH_MAX);
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
  listidx_t iteridx;

  mkc_message (MKC_V_TMI, "chk-dep-ts: %s ", filepath);

  if (! sv_is_defined (target->sv, SV_T_BUILD_DATA,
      filepath, MKC_C_BVAR_DEPENDENCY)) {
    mkc_message (MKC_V_TMI, "ood\n");
    return TARGET_OUT_OF_DATE;
  }

  fts = 0;
  if (sv_is_defined (
      target->sv, SV_T_BUILD_DATA, filepath, MKC_C_BVAR_TIMESTAMP)) {
    fts = sv_get_timestamp (target->sv, SV_T_BUILD_DATA, filepath);
  }

  target_iter_dependency_ts_start (target, filepath, &iteridx);
  while ((ts = target_iter_dependency_ts (target, filepath, &iteridx)) != MKC_ITER_FINISH) {
    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    if (ts > fts) {
      mkc_message (MKC_V_TMI, "ood\n");
      return TARGET_OUT_OF_DATE;
    }
  }

  mkc_message (MKC_V_TMI, "curr\n");
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
  list_t      * elist;
  value_t         evalue;
  char            * p;
  bool            first = true;

  mkc_message (MKC_V_TMI, "get-deps: %s\n", filepath);

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

  sv_delete (target->sv, SV_T_BUILD_DATA, tgtname, MKC_C_BVAR_DEPENDENCY);

  /* the dependency list must exist */
  elist = list_init (MKC_LIST_UNSORTED, NULL, NULL, sizeof (value_t), target->mkcerr);
  evalue.vtype = MKC_VT_LIST;
  evalue.list = elist;
  sv_set (target->sv, SV_T_BUILD_DATA, tgtname, MKC_C_BVAR_DEPENDENCY,
      &evalue, MKC_VCTXT_MKC);
  list_free (elist);

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
      sv_append_str_list (target->sv, SV_T_BUILD_DATA,
          tgtname, MKC_C_BVAR_DEPENDENCY, p, MKC_VCTXT_MKC);
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
list_t *
target_get_include_list (target_t *target, chararr_t * include_paths,
    mkc_regex_t *rx, int64_t *ts)
{
  value_t         * valhdr = NULL;
  list_t          * hlist = NULL;
#if _have_regex
  char            * hdrpath = NULL;
  char            * tname = NULL;
  char            * tend = NULL;
  scopedvar_t     * sv = NULL;
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

  sv = target->sv;

  memcpy (tname, "matchil_", 8);
  if (sv_is_defined (sv, SV_T_LOCAL, tname, NULL)) {
    valhdr = sv_get_value (sv, SV_T_LOCAL, tname, NULL);
    hlist = valhdr->list;
    memcpy (tname, "matchts_", 8);
    if (sv_is_defined (sv, SV_T_LOCAL, tname, NULL)) {
      int64_t   cachedts;

      cachedts = sv_get_timestamp (sv, SV_T_LOCAL, tname);
      *ts = cachedts;
      free (tname);
      return hlist;
    }
    memcpy (tname, "matchil_", 8);
    sv_delete (sv, SV_T_LOCAL, tname, NULL);
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
    list_t    *tlist = NULL;
    listidx_t iteridx;
    listidx_t idx;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    /* all of the header files for this path */
    tlist = dir_match (path, rx, target->mkcerr);

    list_iter_start (tlist, &iteridx);
    while ((idx = list_iter_next (tlist, &iteridx)) != MKC_ITER_FINISH) {
      char          ** temp;
      char          * hdr;
      int64_t       tts;

      if (mkc_error_chk_err (target->mkcerr)) {
        break;
      }

      temp = list_get_by_idx (tlist, idx);
      hdr = *temp;

      if (strcmp (path, ".") == 0) {
        stpecpy (hdrpath, hdrpath + MKC_PATH_MAX, hdr);
      } else {
        snprintf (hdrpath, MKC_PATH_MAX, "%s/%s", path, hdr);
      }
      sv_set_str (sv, SV_T_PATHS, hdr, NULL, hdrpath, MKC_VCTXT_MKC);
      tts = fileop_modtime (hdrpath);

      /* cache invalidation check */
      /* if the timestamp of the file is newer than the cached timestamp */
      /* for the header, then the list of dependencies for the header */
      /* is out of date and must be cleared */
      /* header dependencies are used for the 'check_include_...' tests */
      if (sv_is_defined (sv, SV_T_BUILD_DATA, hdrpath, MKC_C_BVAR_TIMESTAMP)) {
        int64_t     cachedts;

        cachedts = sv_get_timestamp (sv, SV_T_BUILD_DATA, hdrpath);
        if (tts > cachedts) {
          sv_delete (sv, SV_T_BUILD_DATA, hdrpath, MKC_C_BVAR_DEPENDENCY);
        }
      }

      sv_set_timestamp (sv, SV_T_BUILD_DATA, hdrpath, MKC_C_BVAR_TIMESTAMP,
          tts, MKC_VCTXT_MKC);
      sv_set_integer (sv, SV_T_BUILD_DATA, hdrpath, MKC_C_BVAR_TYPE,
          TGT_T_INCLUDE, MKC_VCTXT_MKC);
      if (tts > *ts) {
        sv_append_str_list (sv, SV_T_LOCAL,
            tname, NULL, hdrpath, MKC_VCTXT_MKC);
      }
      if (tts > newts) {
        newts = tts;
      }
    }

    list_free (tlist);
  }

  memcpy (tname, "matchil_", 8);
  valhdr = sv_get_value (sv, SV_T_LOCAL, tname, NULL);
  hlist = valhdr->list;

  memcpy (tname, "matchts_", 8);
  /* return the timestamp of the latest include file */
  *ts = newts;
  sv_set_timestamp (sv, SV_T_LOCAL, tname, NULL, newts, MKC_VCTXT_MKC);

  free (hdrpath);
  free (tname);
gmkcdebug = false;
#endif
  return hlist;
}

/* used for check_include_dependencies, check_include_guards, */
/*   check_include_compile */
const char *
target_iter_includes (target_t *target, list_t *hlist,
    listidx_t *hiteridx, char *hdrpath, size_t hpsz)
{
  listidx_t   hidx;

  while ((hidx = list_iter_next (hlist, hiteridx)) != MKC_ITER_FINISH) {
    value_t     *tvalue;
    const char  *p;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    tvalue = list_get_by_idx (hlist, hidx);
    sv_value_get_str (target->sv, tvalue, hdrpath, hpsz);
    p = path_filename (hdrpath);
    return p;
  }

  return NULL;
}

void
target_iter_dependency_ts_start (target_t *target, const char *filename,
    listidx_t *iteridx)
{
  value_t     *value;

  value = sv_get_value (target->sv, SV_T_BUILD_DATA, filename, MKC_C_BVAR_DEPENDENCY);
  value_iter_start (value, iteridx);
}

int64_t
target_iter_dependency_ts (target_t *target, const char *filename,
    listidx_t *iteridx)
{
  value_t       *value;
  value_t       tvalue;
  listidx_t didx;
  int64_t       ts;
  char          dep [MKC_VNAME_MAX];

  value = sv_get_value (target->sv, SV_T_BUILD_DATA, filename, MKC_C_BVAR_DEPENDENCY);
  didx = value_iter_next (value, &tvalue, iteridx);
  if (didx == MKC_ITER_FINISH) {
    return didx;
  }

  sv_value_get_str (target->sv, &tvalue, dep, sizeof (dep));
  if (! sv_is_defined (target->sv, SV_T_BUILD_DATA, dep, MKC_C_BVAR_DEPENDENCY)) {
    return 0;
  }

  ts = sv_get_timestamp (target->sv, SV_T_BUILD_DATA, dep);
  return ts;
}

void
target_executable_object (target_t *target, const char *execnm,
    const char *objnm)
{
  char        * epath;
  char        * opath;
  int64_t     fts = 0;
  scopedvar_t * sv = target->sv;
  value_t     * value;
  bool        changed;

  value = sv_get_value (target->sv, SV_T_INTERNAL,
      MKC_C_MKC_CHANGED, NULL);
  changed = sv_value_get_integer (target->sv, value);

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
  sv_set_str (sv, SV_T_PATHS, execnm, NULL, epath, MKC_VCTXT_MKC);
  fts = fileop_modtime (epath);
  sv_set_timestamp (sv, SV_T_BUILD_DATA, epath, MKC_C_BVAR_TIMESTAMP, fts, MKC_VCTXT_MKC);
  sv_set_integer (sv, SV_T_BUILD_DATA, epath, MKC_C_BVAR_TYPE, TGT_T_EXEC, MKC_VCTXT_MKC);

  path_build (MKC_PATH_MKCF_OBJECTS, opath, MKC_PATH_MAX, objnm, target->mkcerr);
  sv_set_str (sv, SV_T_PATHS, objnm, NULL, opath, MKC_VCTXT_MKC);
  fts = fileop_modtime (opath);
  sv_set_timestamp (sv, SV_T_BUILD_DATA, opath, MKC_C_BVAR_TIMESTAMP, fts, MKC_VCTXT_MKC);
  sv_set_integer (sv, SV_T_BUILD_DATA, opath, MKC_C_BVAR_TYPE, TGT_T_OBJECT, MKC_VCTXT_MKC);

  mkc_log (target->log, MKC_LOG_TARGET, "  %s\n", objnm);
  if (changed) {
    sv_append_str_list (sv, SV_T_BUILD_DATA,
        epath, MKC_C_BVAR_DEPENDENCY, opath, MKC_VCTXT_MKC);
  }

  free (epath);
  free (opath);
  // opath is set in the list and should not be freed
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
  listidx_t   diteridx;
  listidx_t   didx;
  char            * path;
  char            * opath;
  chararr_t       * cflags;

  opath = malloc (MKC_PATH_MAX);
  if (opath == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  value = sv_get_value (target->sv, SV_T_PATHS, objnm, NULL);
  sv_value_get_str (target->sv, value, opath, MKC_PATH_MAX);

  mkc_log (target->log, MKC_LOG_TARGET, "object-file: %s %s\n", objnm, srcname);

  if (target_check_dependency_timestamp (
      target, objnm, opath) == TARGET_OUT_OF_DATE) {
    mkc_message (MKC_V_INFO, "-- getting dependencies for %s\n", objnm);
    cflags = target_get_flags (target, MKC_C_CFLAGS, NULL);
    target_get_dependencies (target,
        target->attr->currcompiler, opath, srcname, tgtflags, cflags);
    chararr_free (cflags);
  }

  valdeplist = sv_get_value (target->sv, SV_T_BUILD_DATA,
      opath, MKC_C_BVAR_DEPENDENCY);
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

  sv_set_integer (target->sv, SV_T_BUILD_DATA, srcname, MKC_C_BVAR_TYPE,
      TGT_T_SOURCE, MKC_VCTXT_MKC);

  value_iter_start (valdeplist, &diteridx);
  while ((didx = value_iter_next (valdeplist, &tvalue, &diteridx)) != MKC_ITER_FINISH) {
    char        dep [MKC_VNAME_MAX];

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    sv_value_get_str (target->sv, &tvalue, dep, sizeof (dep));
    target_process_timestamp (target, path, MKC_PATH_MAX, dep);
  }

  free (path);
  free (opath);
  return;
}

void
target_build (target_t *target, list_t *blist)
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

// ### need to set the target profile in the hierarchy

  cflags = target_get_flags (target, MKC_C_CFLAGS, NULL);
  ldflags = target_get_flags (target, MKC_C_LDFLAGS, NULL);
  libs = target_get_flags (target, MKC_C_LIBS, NULL);

  toposort_iter_start (topo);
  while ((builditem = toposort_iter_next_reverse (topo)) != NULL) {
    value_t         *value;
    value_t         * valdeplist;
    value_t         tvalue;
    listidx_t   diteridx;
    listidx_t   didx;
    int             tgttype;
    ct_type_t       comptype = COMPILE_COMPILE;
    const char      *buildtag = "";

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    value = sv_get_value (target->sv, SV_T_BUILD_DATA,
        builditem, MKC_C_BVAR_TYPE);
    if (value == NULL) {
      continue;
    }

    tgttype = sv_value_get_integer (target->sv, value);
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

    valdeplist = sv_get_value (target->sv, SV_T_BUILD_DATA,
        builditem, MKC_C_BVAR_DEPENDENCY);
    value_iter_start (valdeplist, &diteridx);
    while ((didx = value_iter_next (valdeplist, &tvalue, &diteridx)) != MKC_ITER_FINISH) {
      int       ttgttype;

      if (mkc_error_chk_err (target->mkcerr)) {
        break;
      }

      sv_value_get_str (target->sv, &tvalue, dep, MKC_PATH_MAX);
      value = sv_get_value (target->sv, SV_T_BUILD_DATA, dep, MKC_C_BVAR_TYPE);

      if (value == NULL) {
        continue;
      }

      ttgttype = sv_value_get_integer (target->sv, value);
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
      sv_set_timestamp (target->sv, SV_T_BUILD_DATA, builditem,
          MKC_C_BVAR_TIMESTAMP, tts, MKC_VCTXT_MKC);
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

  if (sv_is_defined (target->sv, SV_T_BUILD_DATA, filename, MKC_C_BVAR_TIMESTAMP)) {
    return;
  }

  stpecpy (path, path + psz, filename);
  if (*path != '/') {
    value = sv_get_value (target->sv, SV_T_PATHS, filename, NULL);
    if (value == NULL) {
// ### need the set of paths from cflags
    } else {
      sv_value_get_str (target->sv, value, path, psz);
    }
  }

  ts = fileop_modtime (path);
  sv_set_timestamp (target->sv, SV_T_BUILD_DATA, filename, MKC_C_BVAR_TIMESTAMP, ts, MKC_VCTXT_MKC);
}

static void
target_topo_add_items_deps (target_t *target, toposort_t *topo,
    list_t *itemlist)
{
  listidx_t   iteridx;
  listidx_t   idx;
  char            * itemnm;
  list_t      * ilist;

  itemnm = malloc (MKC_PATH_MAX);
  if (itemnm == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  ilist = list_init (MKC_LIST_UNSORTED, NULL, NULL, sizeof (value_t), target->mkcerr);

  list_iter_start (itemlist, &iteridx);
  while ((idx = list_iter_next (itemlist, &iteridx)) != MKC_ITER_FINISH) {
    value_t   * value;
    value_t   * tvalue;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    value = list_get_by_idx (itemlist, idx);
    sv_value_get_str (target->sv, value, itemnm, MKC_PATH_MAX);

    tvalue = sv_get_value (target->sv, SV_T_PATHS, itemnm, NULL);
    if (tvalue == NULL) {
      tvalue = value;
    }
    list_set (ilist, tvalue);
  }
  target_topo_add_items (target, topo, ilist);

  list_iter_start (ilist, &iteridx);
  while ((idx = list_iter_next (ilist, &iteridx)) != MKC_ITER_FINISH) {
    value_t     * value;
    value_t     * valdeplist;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    value = list_get_by_idx (ilist, idx);
    sv_value_get_str (target->sv, value, itemnm, MKC_PATH_MAX);

    valdeplist = sv_get_value (target->sv, SV_T_BUILD_DATA,
        itemnm, MKC_C_BVAR_DEPENDENCY);
    if (valdeplist == NULL) {
      /* items without dependency lists are include files */
      continue;
    }

    value = sv_get_value (target->sv, SV_T_BUILD_DATA, itemnm, MKC_C_BVAR_TYPE);
    if (value != NULL) {
      int             tgttype;

      tgttype = sv_value_get_integer (target->sv, value);
      if (tgttype == TGT_T_INCLUDE) {
        continue;
      }
    }

    target_topo_add_items_deps (target, topo, valdeplist->list);

    if (! sv_is_defined (target->sv, SV_T_BUILD_DATA, itemnm, MKC_C_BVAR_DEPENDENCY)) {
      continue;
    }

    target_topo_add_deps (target, topo, itemnm);
  }

  list_free (ilist);
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
