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
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include "alternate.h"
#include "attribute.h"
#include "chararr.h"
#include "compile.h"
#include "const.h"
#include "dirmatch.h"
#include "dirop.h"
#include "fileop.h"
#include "mkc_compiler.h"
#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"
#include "mkc_util.h"
#include "mkc_util.h"
#include "os_process.h"
#include "pathutil.h"
#include "scopedvar.h"
#include "strutil.h"
#include "target.h"
#include "tmppath.h"
#include "tmutil.h"
#include "toposort.h"
#include "value.h"

typedef struct target_t {
  scopedvar_t         * sv;
  compile_t           * compile;
  mkc_attribute_t     * attr;
  mkc_log_t           * log;
  mkc_error_t         * mkcerr;
  tmppath_t           * tmppath;
  bool                create_stage_bin;
  bool                mkc_changed;
} target_t;

typedef enum {
  TGT_FLAGS,
  TGT_ADDITIONAL,
} tgt_flag_t;

static const char * const dependency_delim = " \n\r\\";

static bool target_chk_last_libloc (mkc_compiler_id_t compid, char *lastlibloc, size_t sz, const char *str);
static void target_topo_add_items_deps (target_t *target, toposort_t *topo, list_t *itemlist);
static void target_create_stage_bin (target_t *target);
static void target_set_target_flags (target_t *target, const char *builditem, const char *bvarname, comp_flag_t ftype);
static void target_append_flags (target_t *target, comp_flag_t ftype, value_t *value, chararr_t *flags, chararr_t *include_paths);
static int64_t target_get_timestamp (target_t *target, const char *filepath);
static void target_update_timestamp (target_t *target, const char *filepath);

MKC_NODISCARD
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
  target->mkc_changed = false;
  target->tmppath = tmppath_init (mkcerr, 5);

  return target;
}

void
target_free (target_t *target)
{
  if (target == NULL) {
    return;
  }

  tmppath_free (target->tmppath);
  free (target);
}

MKC_NODISCARD
chararr_t *
target_get_flags (target_t *target, const char *flagname,
    chararr_t *include_paths)
{
  scopedvar_t     * sv;
  sv_iter_t       * sviter = NULL;
  const char      * profnm;
  chararr_t       * flags;

  sv = target->sv;

  flags = chararr_init (target->mkcerr);
  if (flags == NULL) {
    return NULL;
  }
  chararr_set_freeinternals (flags);

  sviter = sv_iter_start (sv, SV_ITER_HIERARCHY);
  while ((profnm = sv_iter_next (sv, sviter)) != NULL) {
    value_t     * value = NULL;
    sv_type_t   svtype;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    svtype = sv_iter_get_type (sv, sviter);
    value = sv_get_value (sv, svtype, flagname, NULL);
    if (value == NULL || value->vtype != MKC_VT_LIST) {
      continue;
    }

    target_append_flags (target, COMP_COMPFLAGS, value, flags, include_paths);
  }
  sv_iter_finish (sviter);
  chararr_append (flags, NULL);

  return flags;
}

void
target_set_changed (target_t *target, bool changed)
{
  if (target == NULL) {
    return;
  }

  target->mkc_changed = changed;
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
  value_t     * valdeplist;
  listidx_t   diteridx;
  listidx_t   didx;
  value_t     tvalue;
  char        * dep;

  mkc_log (target->log, MKC_LOG_CHECK, "add-dep %s\n", tgtname);
  valdeplist = sv_get_value (target->sv, SV_T_BUILD_DATA,
      tgtname, MKC_C_BVAR_DEPENDENCY);
  if (valdeplist == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_FATAL_ERROR, 0, tgtname);
    fprintf (stderr, "ERR: unable to locate dep list for %s\n", tgtname);
    return;
  }

  dep = tmppath_get (target->tmppath);
  *dep = '\0';

  value_iter_start (valdeplist, &diteridx);
  while ((didx = value_iter_next (valdeplist, &tvalue, &diteridx)) != MKC_ITER_FINISH) {
    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    sv_value_get_str (target->sv, &tvalue, dep, MKC_PATH_MAX);
    mkc_log (target->log, MKC_LOG_CHECK, "  %s\n", dep);
    if (strcmp (tgtname, dep) == 0) {
      continue;
    }
    toposort_add_pair (topo, tgtname, dep);
  }

  tmppath_release (target->tmppath, dep);
}

int
target_check_dependency_timestamp (target_t *target,
    const char *filepath)
{
  int64_t       fts;
  int64_t       ts;
  listidx_t iteridx;


  mkc_message (MKC_V_TMI, "chk-dep-ts: %s\n", filepath);
  if (! sv_is_defined (target->sv, SV_T_BUILD_DATA,
      filepath, MKC_C_BVAR_DEPENDENCY)) {
    mkc_message (MKC_V_TMI, "chk-dep-ts: %s no-dep-def ood\n", filepath);
    return TARGET_OUT_OF_DATE;
  }

  fts = target_get_timestamp (target, filepath);
  if (fts == 0) {
    mkc_message (MKC_V_TMI, "chk-dep-ts: %s 0 ood\n", filepath);
    /* can short-circuit any filepath that does not have a timestamp set */
    return TARGET_OUT_OF_DATE;
  }

  mkc_message (MKC_V_WTMI, "chk-dep-ts: %s fts: %" PRId64 "\n", filepath, fts);
  target_iter_dependency_ts_start (target, filepath, &iteridx);
  while ((ts = target_iter_dependency_ts (target, filepath, &iteridx)) != MKC_ITER_FINISH) {
    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    if (ts == 0 || ts >= fts) {
      mkc_message (MKC_V_TMI, "chk-dep-ts: %s ts-chk ood\n", filepath);
      return TARGET_OUT_OF_DATE;
    }
  }

  mkc_message (MKC_V_TMI, "chk-dep-ts: %s curr\n", filepath);
  return TARGET_CURRENT;
}

void
target_get_dependencies (target_t *target,
    mkc_compiler_t compiler, const char *tgtname, const char *filepath,
    target_flag_t flags, chararr_t *compflags)
{
  int             rc;
  char            * rbuff;
  size_t          rsz;
  char            * tokstr;
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
    compile_append_flag (target->compile, COMP_COMPFLAGS,
        compiler_get_flag (target->attr->compid, MKC_COMP_FLAG_DEPS_USER));
  } else {
    compile_append_flag (target->compile, COMP_COMPFLAGS,
        compiler_get_flag (target->attr->compid, MKC_COMP_FLAG_DEPS));
  }
  compile_append_flag (target->compile, COMP_COMPFLAGS, NULL);
  compile_preprocess (target->compile);
  compile_set_flags (target->compile, compflags, NULL, NULL);
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
  sv_create_empty_list (target->sv, SV_T_BUILD_DATA,
      tgtname, MKC_C_BVAR_DEPENDENCY, MKC_VCTXT_MKC);

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
MKC_NODISCARD
list_t *
target_get_include_list (target_t *target, chararr_t * include_paths,
    mkc_regex_t *rx, int64_t *ts)
{
  value_t       * tvalue = NULL;
  list_t        * hlist = NULL;
#if _have_regex
  char          * hdrpath = NULL;
  char          * tname = NULL;
  char          * tend = NULL;
  scopedvar_t   * sv = NULL;
  const char    ** patharr;
  const char    * path = NULL;
  int64_t       newts = 0;
  size_t        tnamesz = MKC_SMALL_BUFF_SZ;
  char          * p;
  int           count;

  tname = malloc (tnamesz);
  if (tname == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  *tname = '\0';

  /* messy stuff to cache the include timestamps and include list */
  /* this cache is used for the check_include_* statements */

  /* matchts_ and matchil_ are only used internally, */
  /* there is no need to "clean" the */
  /* variable name, and cleaning it makes it much less specific */
  p = tname;
  tend = tname + tnamesz;
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
  tvalue = sv_get_value (sv, SV_T_LOCAL, tname, NULL);
  if (tvalue != NULL) {
    hlist = tvalue->list;
    memcpy (tname, "matchts_", 8);
    tvalue = sv_get_value (sv, SV_T_LOCAL, tname, NULL);
    if (tvalue != NULL) {
      int64_t   cachedts;

      cachedts = sv_value_get_timestamp (sv, tvalue);
      *ts = cachedts;
      free (tname);
      return hlist;
    }
    memcpy (tname, "matchil_", 8);
    sv_delete (sv, SV_T_LOCAL, tname, NULL);
  }

  hdrpath = tmppath_get (target->tmppath);
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
      tts = target_get_timestamp (target, hdrpath);

      /* cache invalidation check */
      /* if the timestamp of the file is newer than the cached timestamp */
      /* for the header, then the list of dependencies for the header */
      /* is out of date and must be cleared */
      /* header dependencies are used for the 'check_include_...' tests */

      tvalue = sv_get_value (sv, SV_T_BUILD_DATA, hdrpath, MKC_C_BVAR_TIMESTAMP);
      if (tvalue != NULL) {
        int64_t     cachedts;

        cachedts = sv_value_get_timestamp (sv, tvalue);
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
  tvalue = sv_get_value (sv, SV_T_LOCAL, tname, NULL);
  hlist = tvalue->list;

  memcpy (tname, "matchts_", 8);
  /* return the timestamp of the latest include file */
  *ts = newts;
  sv_set_timestamp (sv, SV_T_LOCAL, tname, NULL, newts, MKC_VCTXT_MKC);

  tmppath_release (target->tmppath, hdrpath);
  free (tname);
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
target_iter_dependency_ts (target_t *target, const char *filepath,
    listidx_t *iteridx)
{
  value_t       *value;
  value_t       tvalue;
  listidx_t didx;
  int64_t       ts;
  char          dep [MKC_VNAME_MAX];

  value = sv_get_value (target->sv, SV_T_BUILD_DATA, filepath, MKC_C_BVAR_DEPENDENCY);
  didx = value_iter_next (value, &tvalue, iteridx);
  if (didx == MKC_ITER_FINISH) {
    return didx;
  }

  sv_value_get_str (target->sv, &tvalue, dep, sizeof (dep));
  mkc_message (MKC_V_WTMI, "dep: %s ", dep);

  ts = target_get_timestamp (target, dep);
  mkc_message_no_indent (MKC_V_WTMI, "%" PRId64 "\n", ts);
  return ts;
}

void
target_create_target (target_t *target, const char *path, int tgttype)
{
  target_get_timestamp (target, path);
  sv_set_integer (target->sv, SV_T_BUILD_DATA, path,
      MKC_C_BVAR_TYPE, tgttype, MKC_VCTXT_MKC);
}

void
target_create_dependency (target_t *target, const char *path,
    const char *deppath)
{
  scopedvar_t * sv = target->sv;

  mkc_log (target->log, MKC_LOG_TARGET, "create-dep: %s %s\n", path, deppath);

  sv_append_str_list (sv, SV_T_BUILD_DATA,
      path, MKC_C_BVAR_DEPENDENCY, deppath, MKC_VCTXT_MKC);

  return;
}

void
target_object_source (target_t *target, const char *objpath,
    const char *srcname)
{
  target_flag_t   tgtflags = TARGET_NONE;
  value_t         tvalue;
  value_t         * valdeplist;
  listidx_t       diteridx;
  listidx_t       didx;

  mkc_log (target->log, MKC_LOG_TARGET, "object-file: %s %s\n", objpath, srcname);

  if (target_check_dependency_timestamp (
      target, objpath) == TARGET_OUT_OF_DATE) {
    chararr_t   * compflags;

    mkc_message (MKC_V_INFO, "getting dependencies for %s\n", objpath);
    compflags = target_get_flags (target, MKC_C_COMPFLAGS, NULL);
    target_get_dependencies (target,
        target->attr->currcompiler, objpath, srcname, tgtflags, compflags);
    chararr_free (compflags);
  }

  valdeplist = sv_get_value (target->sv, SV_T_BUILD_DATA,
      objpath, MKC_C_BVAR_DEPENDENCY);
  if (valdeplist == NULL) {
    mkc_error_set (target->mkcerr, MKC_ERR_FATAL_ERROR, 0, "missing dependency list");
    mkc_log (target->log, MKC_LOG_ERROR, "ERR: %s dependency list not found\n", objpath);
    return;
  }

  sv_set_integer (target->sv, SV_T_BUILD_DATA, srcname, MKC_C_BVAR_TYPE,
      TGT_T_SOURCE, MKC_VCTXT_MKC);

  value_iter_start (valdeplist, &diteridx);
  while ((didx = value_iter_next (valdeplist, &tvalue, &diteridx)) != MKC_ITER_FINISH) {
    char        dep [MKC_VNAME_MAX];
    value_t     * tmpvalue;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    sv_value_get_str (target->sv, &tvalue, dep, sizeof (dep));
    tmpvalue = sv_get_value (target->sv, SV_T_PATHS, dep, NULL);
    if (tmpvalue != NULL) {
      sv_value_get_str (target->sv, tmpvalue, dep, sizeof (dep));
    }
    target_get_timestamp (target, dep);
  }

  return;
}

/* if the configuration method is 'auto', 'infile' is NULL */
int
target_config_file (target_t *target, const char *outfile,
    const char *mkcfile, const char *infile)
{
  int64_t         fts;
  int64_t         ts;

  mkc_log (target->log, MKC_LOG_TARGET, "conf-file: %s\n", outfile);

  fts = target_get_timestamp (target, outfile);
  mkc_message (MKC_V_TMI, "config: %" PRId64 " (out)\n", fts);
  ts = target_get_timestamp (target, mkcfile);
  if (fts == 0 || ts > fts) {
    mkc_message (MKC_V_TMI, "config: %" PRId64 " (mkc) > " PRId64 " %s\n", ts, fts, mkcfile);
    return TARGET_OUT_OF_DATE;
  }

  if (infile != NULL) {
    ts = target_get_timestamp (target, infile);
    if (ts > fts) {
      mkc_message (MKC_V_TMI, "config: %" PRId64 " (in)  > " PRId64 " %s\n", ts, fts, infile);
      return TARGET_OUT_OF_DATE;
    }
  }

  return TARGET_CURRENT;
}

void
target_build (target_t *target, list_t *blist)
{
  toposort_t    * topo;
  const char    * builditem;
  char          * dep;
  char          * source;
  int           rc;
  chararr_t     * compflags;
  chararr_t     * linkflags;
  chararr_t     * libs;

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

  dep = tmppath_get (target->tmppath);
  *dep = '\0';
  source = tmppath_get (target->tmppath);
  *source = '\0';

  compflags = target_get_flags (target, MKC_C_COMPFLAGS, NULL);
  linkflags = target_get_flags (target, MKC_C_LINKFLAGS, NULL);
  libs = target_get_flags (target, MKC_C_LIBS, NULL);

  toposort_iter_start (topo);
  while ((builditem = toposort_iter_next_reverse (topo)) != NULL) {
    value_t       * value;
    value_t       * valdeplist;
    value_t       tvalue;
    listidx_t     diteridx;
    listidx_t     didx;
    int           tgttype;
    comp_type_t   comptype = COMPILE_COMPILE;
    const char    * buildtag = "";

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
      case TGT_T_EXEC:
      case TGT_T_USER_EXEC: {
        target_create_stage_bin (target);
        compile_set_output (target->compile, builditem);
        comptype = COMPILE_LINK;
        buildtag = "link";
        break;
      }
      case TGT_T_INCLUDE: {
        continue;
      }
      case TGT_T_LIBRARY:
      case TGT_T_USER_LIBRARY: {
        break;
      }
      case TGT_T_FILE:
      case TGT_T_USER_FILE: {
        buildtag = "create";
        break;
      }
      case TGT_T_OBJECT:
      case TGT_T_USER_OBJECT: {
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
          target, builditem) == TARGET_CURRENT) {
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
      if ((tgttype == TGT_T_EXEC ||
          tgttype == TGT_T_USER_EXEC) &&
          (ttgttype == TGT_T_OBJECT ||
          ttgttype == TGT_T_USER_OBJECT)) {
        char    *tmp;

        tmp = strdup (dep);
        compile_append_object (target->compile, tmp);
      }
      if ((tgttype == TGT_T_OBJECT ||
          tgttype == TGT_T_USER_OBJECT) &&
          ttgttype == TGT_T_SOURCE) {
        stpecpy (source, source + MKC_PATH_MAX, dep);
      }
    }

    if (tgttype == TGT_T_EXEC ||
        tgttype == TGT_T_USER_EXEC ||
        tgttype == TGT_T_LIBRARY ||
        tgttype == TGT_T_USER_LIBRARY ||
        tgttype == TGT_T_OBJECT ||
        tgttype == TGT_T_USER_OBJECT) {
      int         rc;

      compile_append_object (target->compile, NULL);
      compile_set_flags (target->compile, compflags, linkflags, libs);

      target_set_target_flags (target, builditem,
          MKC_C_BVAR_COMPFLAGS, COMP_COMPFLAGS);
      target_set_target_flags (target, builditem,
          MKC_C_BVAR_LINKFLAGS, COMP_LINKFLAGS);
      target_set_target_flags (target, builditem,
          MKC_C_BVAR_LIBS, COMP_LIBS);

      target->attr->printerrors = true;
      rc = compile_exec (target->compile, comptype, target->attr->currcompiler,
          source, NULL, 0);
      compile_reset (target->compile);
      if (rc != MKC_OK) {
        continue;
      }

      target_update_timestamp (target, builditem);
    }
  }

  chararr_free (compflags);
  chararr_free (linkflags);
  chararr_free (libs);

  toposort_free (topo);
  tmppath_release (target->tmppath, dep);
  tmppath_release (target->tmppath, source);
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
target_topo_add_items_deps (target_t *target, toposort_t *topo,
    list_t *itemlist)
{
  listidx_t   iteridx;
  listidx_t   idx;
  char        * itemnm;
  list_t      * ilist;

  itemnm = tmppath_get (target->tmppath);

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
  tmppath_release (target->tmppath, itemnm);
}

static void
target_create_stage_bin (target_t *target)
{
  char    *epath;

  if (target->create_stage_bin) {
    return;
  }

  epath = tmppath_get (target->tmppath);
  path_build (MKC_PATH_STAGE_BIN, epath, MKC_PATH_MAX, NULL, target->mkcerr);
  dirop_make (epath, target->mkcerr);
  target->create_stage_bin = true;
  tmppath_release (target->tmppath, epath);
}

static void
target_set_target_flags (target_t *target, const char *builditem,
    const char *bvarname, comp_flag_t ftype)
{
  value_t   * value;

  value = sv_get_value (target->sv, SV_T_BUILD_DATA,
      builditem, bvarname);
  if (value == NULL || value->vtype != MKC_VT_LIST) {
    return;
  }

  target_append_flags (target, ftype, value, NULL, NULL);
}

static void
target_append_flags (target_t *target, comp_flag_t ftype, value_t *value,
    chararr_t *flags, chararr_t *include_paths)
{
  listidx_t   fiter;
  listidx_t   fidx;
  bool        append_next = false;
  char        * str;
  char        * lastlibloc;
  const char  * incflag;
  size_t      incflaglen;

  incflag = compiler_get_flag (target->attr->compid, MKC_COMP_FLAG_INCLUDE);
  incflaglen = compiler_get_flag_len (target->attr->compid, MKC_COMP_FLAG_INCLUDE);

  lastlibloc = tmppath_get (target->tmppath);
  *lastlibloc = '\0';
  str = tmppath_get (target->tmppath);
  *str = '\0';

  list_iter_start (value->list, &fiter);
  while ((fidx = list_iter_next (value->list, &fiter)) != MKC_ITER_FINISH) {
    value_t   *fval;

    if (mkc_error_chk_err (target->mkcerr)) {
      break;
    }

    fval = list_get_by_idx (value->list, fidx);
    sv_value_get_str (target->sv, fval, str, MKC_PATH_MAX);
    if (! *str) {
      continue;
    }
    if (target_chk_last_libloc (target->attr->compid,
        lastlibloc, MKC_PATH_MAX, str)) {
      /* de-duplication check */
      continue;
    }

    if (include_paths != NULL) {
      if (append_next) {
        if (*str != '/') {
          chararr_append (include_paths, strdup (str));
        }
        append_next = false;
      }
      if (strncmp (str, incflag, incflaglen) == 0) {
        if (strcmp (str, incflag) == 0) {
          append_next = true;
        } else {
          if (*(str + incflaglen) != '/') {
            chararr_append (include_paths, strdup (str + incflaglen));
          }
        }
      }
    }

    if (flags != NULL) {
      chararr_append (flags, strdup (str));
    } else {
      compile_set_freeinternals (target->compile, ftype);
      compile_append_flag (target->compile, ftype, strdup (str));
    }
  }

  if (flags == NULL) {
    compile_append_flag (target->compile, ftype, NULL);
  }

  tmppath_release (target->tmppath, lastlibloc);
  tmppath_release (target->tmppath, str);
}

static int64_t
target_get_timestamp (target_t *target, const char *filepath)
{
  value_t     * value;
  int64_t     ts = 0;

  value = sv_get_value (target->sv, SV_T_BUILD_DATA, filepath, MKC_C_BVAR_TIMESTAMP);
  if (value != NULL) {
    ts = sv_get_timestamp (target->sv, SV_T_BUILD_DATA, filepath);
    mkc_message (MKC_V_WTMI, "t: ts-cache: %s ts: %" PRId64 "\n", filepath, ts);
  }
  if (value == NULL || ts == 0) {
    ts = fileop_modtime (filepath);
    sv_set_timestamp (target->sv, SV_T_BUILD_DATA, filepath, MKC_C_BVAR_TIMESTAMP, ts, MKC_VCTXT_MKC);
    mkc_message (MKC_V_WTMI, "t: ts-file: %s ts: %" PRId64 "\n", filepath, ts);
  }

  return ts;
}

static void
target_update_timestamp (target_t *target, const char *filepath)
{
  int64_t     ts = 0;

  ts = fileop_modtime (filepath);
  sv_set_timestamp (target->sv, SV_T_BUILD_DATA, filepath, MKC_C_BVAR_TIMESTAMP, ts, MKC_VCTXT_MKC);
  mkc_message (MKC_V_WTMI, "t: ts-upd: %s ts: %" PRId64 "\n", filepath, ts);
}
