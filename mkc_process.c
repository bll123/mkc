/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#ifndef MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include "asttoken.h"
#include "envutil.h"
#include "fileop.h"
#include "mkc_check.h"
#include "mkc_const.h"
#include "mkc_context.h"
#include "mkc_def.h"
#include "dirmatch.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_option.h"
#include "mkc_process.h"
#include "mkc_regex.h"
#include "strutil.h"
#include "mkc_util.h"
#include "mkc_var.h"      // for debugging
#include "pathutil.h"
#include "scopedvar.h"
#include "tmutil.h"
#include "toposort.h"
#include "value.h"

enum {
  MKC_AUTO_DEFINE_ZERO,
  MKC_AUTO_SKIP_ZERO,
  MKC_INC_READ,
  MKC_INC_NAME_ONLY,
  MKC_INC_PATH_ONLY,
};

enum {
  MKC_CACHE_VERS_1 = 1,
};

typedef struct mkc_user_regex_t {
  char          *pattern;
  mkc_regex_t   *rx;
} mkc_user_regex_t;

/* foreach processing */
typedef struct mkc_foreach_t {
  mkc_list_t      *namelist;
  value_t         *listval;      // list or range
  value_t         tvalue;
  mkc_listidx_t   iteridx;
} mkc_foreach_t;

typedef struct mkc_process_t {
  scopedvar_t       * scopedvar;
  mkc_check_t       * check;
  mkc_context_t     * context;
  mkc_error_t       * mkcerr;
  mkc_log_t         * log;
  mkc_option_t      * mkcoptions;
  char              * projectname;
  const char        * objext;
  const char        * exeext;
  mkc_regex_t       * rxshellvar;
  mkc_regex_t       * rxincguard;
  mkc_list_t        * user_rx_list;
  mkc_attribute_t   attr;
  /* internal */
  mkc_compiler_t    dfltcompiler;
  mkc_system_type_t systype;
  mkc_system_id_t   sysid;
  mkc_compiler_id_t compid;
  mkc_lib_loc_t     libloc;
  mkc_header_t      headertype;
  bool              variadicmacro;
  bool              inloadcache;
  bool              cacheloaded;
  bool              cacheinvalidated;
} mkc_process_t;

static const char *sysnames [MKC_SYS_MAX] = {
  [MKC_SYS_AIX] = "MKC_SYS_AIX",
  [MKC_SYS_ANDROID] = "MKC_SYS_ANDROID",
  [MKC_SYS_BSD] = "MKC_SYS_BSD",
  [MKC_SYS_IOS] = "MKC_SYS_IOS",
  [MKC_SYS_LINUX] = "MKC_SYS_LINUX",
  [MKC_SYS_MACOS] = "MKC_SYS_MACOS",
  [MKC_SYS_SOLARIS] = "MKC_SYS_SOLARIS",
  [MKC_SYS_UNKNOWN] = "MKC_SYS_UNKNOWN",
  [MKC_SYS_WINDOWS] = "MKC_SYS_WINDOWS",
};

static const char *sysidnames [MKC_SYS_ID_MAX] = {
  [MKC_SYS_ID_ALPINE] = "MKC_SYS_ID_ALPINE",
  [MKC_SYS_ID_AZURE] = "MKC_SYS_ID_AZURE",
  [MKC_SYS_ID_ARCH] = "MKC_SYS_ID_ARCH",
  [MKC_SYS_ID_CYGWIN] = "MKC_SYS_ID_CYGWIN",
  [MKC_SYS_ID_DEBIAN] = "MKC_SYS_ID_DEBIAN",
  [MKC_SYS_ID_DRAGONFLYBSD] = "MKC_SYS_ID_DRAGONFLYBSD",
  [MKC_SYS_ID_FEDORA] = "MKC_SYS_ID_FEDORA",
  [MKC_SYS_ID_FREEBSD] = "MKC_SYS_ID_FREEBSD",
  [MKC_SYS_ID_GENTOO] = "MKC_SYS_ID_GENTOO",
  [MKC_SYS_ID_MSYS2] = "MKC_SYS_ID_MSYS2",
  [MKC_SYS_ID_NETBSD] = "MKC_SYS_ID_NETBSD",
  [MKC_SYS_ID_NIXOS] = "MKC_SYS_ID_NIXOS",
  [MKC_SYS_ID_NOTSET] = "MKC_SYS_ID_NOTSET",
  [MKC_SYS_ID_OPENBSD] = "MKC_SYS_ID_OPENBSD",
  [MKC_SYS_ID_SLACKWARE] = "MKC_SYS_ID_SLACKWARE",
  [MKC_SYS_ID_SOLARIS] = "MKC_SYS_ID_SOLARIS",
  [MKC_SYS_ID_SUSE] = "MKC_SYS_ID_SUSE",
  [MKC_SYS_ID_WRLINUX] = "MKC_SYS_ID_WRLINUX",
};

static const char *compidnames [MKC_COMP_ID_MAX] = {
  [MKC_COMP_ID_CLANG] = "MKC_COMP_ID_CLANG",
  [MKC_COMP_ID_GCC] = "MKC_COMP_ID_GCC",
  [MKC_COMP_ID_ICC] = "MKC_COMP_ID_ICC",
  [MKC_COMP_ID_MSC] = "MKC_COMP_ID_MSC",
  [MKC_COMP_ID_SOLARIS] = "MKC_COMP_ID_SOLARIS",
  [MKC_COMP_ID_UNKNOWN] = "MKC_COMP_ID_UNKNOWN",
  [MKC_COMP_ID_XLC] = "MKC_COMP_ID_XLC",
};

static mkc_ctxt_val_t attrcontext [MKC_ATTR_MAX] = {
  [MKC_ATTR_INPUT] = MKC_CONTEXT_CONFIGURE,
// ### lib version will need to be fixed
  [MKC_ATTR_LIB_VERSION] = MKC_CONTEXT_PROJECT,
  [MKC_ATTR_MATCH] = MKC_CONTEXT_CHK_INC,
  [MKC_ATTR_METHOD] = MKC_CONTEXT_CONFIGURE,
  [MKC_ATTR_NAMESPACE] = MKC_CONTEXT_SET,
  [MKC_ATTR_OUTPUT] = MKC_CONTEXT_CONFIGURE,
  [MKC_ATTR_VCONTEXT] = MKC_CONTEXT_SET,
  [MKC_ATTR_VERSION] = MKC_CONTEXT_PROJECT,
};

enum {
  MKC_NO_VARIADIC_MACRO = false,
  MKC_VARIADIC_MACRO_SUPPORTED = true,
};

static char const * const MKC_C_LIBLOCNAME = "MKC_LIB_LOC_LIB64";
static char const * const MKC_C_SHLIBEXT = "MKC_SHARED_LIBRARY_EXTENSION";
static char const * const MKC_C_OBJEXT = "MKC_OBJECT_EXTENSION";
static char const * const MKC_C_EXEEXT = "MKC_EXECUTABLE_EXTENSION";
static char const * const MKC_C_LOOPLIMIT = "MKC_LOOP_LIMIT";
static char const * const MKC_C_IVARMACRO = "MKC_I_VARIADIC_MACRO";
static char const * const MKC_C_PROJECT_NAME = "MKC_PROJECT_NAME";
static char const * const MKC_C_PROJECT_VERS = "MKC_PROJECT_VERSION";
static char const * const MKC_C_PROJECT_LIB_VERS = "MKC_PROJECT_LIBRARY_VERSION";
static char const * const MKC_C_PATH = "MKC_PATH";
static char const * const MKC_C_CHK_INC_COMPILE_TS = "MKC_CHK_INC_COMPILE_TS";
static char const * const MKC_C_CHK_INC_DEPS_TS = "MKC_CHK_INC_DEPS_TS";
static char const * const MKC_C_CHK_INC_GUARDS_TS = "MKC_CHK_INC_GUARDS_TS";
static char ** mkc_process_get_flags (mkc_process_t *process, const char *flagname);
static void process_save_cache_profile (mkc_process_t *process, FILE *fh, char *tbuff, size_t sz, sv_iter_t *sviter, const char *profname, int *tcount);

/* these are duplicated */
/* so that the static aggregator can be initialized */
static char const * const MKC_C_P_PKGCONF = "MKC_PATH_PKGCONF";
static char const * const MKC_C_P_PKGCONFIG = "MKC_PATH_PKG_CONFIG";

typedef struct mkc_prog_chk_t {
  const char  * program;
  const char  * mkcvarname;
} mkc_prog_chk_t;

/* these are executables that are used by mkc */
static mkc_prog_chk_t proglist [] = {
  { "pkgconf",      MKC_C_P_PKGCONF },
  { "pkg-config",   MKC_C_P_PKGCONFIG },
  { NULL,           NULL },
};

static void mkc_process_attr_clear (mkc_process_t *process);
static void mkc_process_user_regex_free (void *turx);
static int mkc_process_user_regex_comp (void *turxa, void *turxb);
const char * mkc_process_create_name (mkc_process_t *process, mkc_astnode_token_t asttype, char *buff, size_t sz, const char *tag, ...);
static int mkc_process_int_checks (mkc_process_t *process);
static void mkc_process_set_defaults (mkc_process_t *process);
static void mkc_process_configure_manual (mkc_process_t *process);
static void mkc_process_configure_auto (mkc_process_t *process, int defzero);
static bool mkc_process_chk_cache (mkc_process_t *process, const char *disp, const char *nm);
static void mkc_process_get_path (mkc_process_t *process);
static void mkc_process_find_executables (mkc_process_t *process);
static mkc_user_regex_t *mkc_process_user_regex_init (mkc_process_t *process, const char *pattern);
static void mkc_process_user_regex_free (void *turx);
static int mkc_process_user_regex_comp (void *turxa, void *turxb);
static void mkc_process_dbg_print_var (mkc_process_t *process, const char *pname);
static void mkc_process_dbg_print_prof (mkc_process_t *process);
static void mkc_process_dbg_print_path (mkc_process_t *process);
static void mkc_process_dbg_print_int_var (mkc_process_t *process);
static char * mkc_process_configure_substitute (mkc_process_t *process, char *data);
static void mkc_process_alternate_free (void *talt);
static void mkc_process_topo_add_items (mkc_process_t *process, toposort_t *topo, mkc_list_t *hlist);
static void mkc_process_topo_add_deps (mkc_process_t *process, toposort_t *topo, char *rbuff, const char *hdr);
static mkc_list_t * mkc_process_get_include_list (mkc_process_t *process, mkc_regex_t *rx, time_t *ts);
static const char * mkc_process_iter_includes (mkc_process_t *process, mkc_list_t *hlist, mkc_listidx_t *hiteridx, char *hdr, size_t hsz);
static bool mkc_process_chk_last_libloc (char *lastlibloc, size_t sz, const char *str);
static void mkc_process_attr_flags (mkc_process_t *process, value_t *value, mkc_list_t *flags, bool inlist);
static void mkc_process_source_file (mkc_process_t *process, const char *target, const char *srcfn);


MKC_NODISCARD
mkc_process_t *
mkc_process_init (scopedvar_t *scopedvar,
    mkc_log_t *log, mkc_context_t *context,
    mkc_option_t *mkcoptions, mkc_error_t *mkcerr)
{
  mkc_process_t     *process;
  int               rc;
  mstime_t          starttm;

  mstimestart (&starttm);
  process = malloc (sizeof (mkc_process_t));

  process->scopedvar = scopedvar;
  /* at this point, the default compiler is not known */
  process->dfltcompiler = MKC_COMPILER_C;
  process->log = log;
  process->context = context;
  process->mkcoptions = mkcoptions;
  process->check = NULL;
  process->objext = ".o";
  process->exeext = "";
  process->projectname = NULL;
  process->rxshellvar = NULL;
  process->rxincguard = NULL;
  process->user_rx_list = mkc_list_init (MKC_LIST_SORTED,
      mkc_process_user_regex_free, mkc_process_user_regex_comp, mkcerr);

  process->attr.currcompiler = process->dfltcompiler;
  process->attr.headertype = process->headertype;
  process->attr.alternates = mkc_list_init (MKC_LIST_UNSORTED, mkc_process_alternate_free, NULL, mkcerr);
  mkc_process_attr_alternate (process);
  process->attr.pathlist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, mkcerr);
  process->attr.replacelist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, mkcerr);
  process->attr.sourcelist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, mkcerr);
  for (int i = 0; i < MKC_ATTR_MAX; ++i) {
    process->attr.str [i] = NULL;
  }
  process->attr.negate = false;
  process->attr.define_zero = MKC_AUTO_SKIP_ZERO;
  process->attr.localheader = false;
  process->attr.printerrors = false;

  process->inloadcache = false;
  process->cacheloaded = false;
  process->cacheinvalidated = false;
  process->mkcerr = mkcerr;

  process->systype = MKC_SYS_UNKNOWN;
  process->sysid = MKC_SYS_ID_NOTSET;
  process->compid = MKC_COMP_ID_UNKNOWN;
  process->libloc = MKC_LIB_LOC_NOTSET;
  process->headertype = MKC_HEADER_MODERN;
  process->attr.headertype = process->headertype;
  process->variadicmacro = MKC_VARIADIC_MACRO_SUPPORTED;

  process->check = mkc_check_init (process->scopedvar,
      &process->attr, log, mkcerr);
  if (process->check == NULL) {
    mkc_process_free (process);
    return NULL;
  }

  mkc_process_set_defaults (process);
  rc = mkc_process_int_checks (process);
  if (rc < 0) {
    mkc_process_free (process);
    return NULL;
  }

  mkc_process_get_path (process);
  mkc_process_find_executables (process);

  {
    char    tbuff [40];
    time_t  etm;

    etm = mstimeend (&starttm);
    mkc_elapsed_disp (etm, tbuff, sizeof (tbuff));
    mkc_message ("-- mkc internal setup: %s\n", tbuff);
    mkc_log (process->log, MKC_LOG_STATISTICS,
        "-- mkc internal setup: %s\n", tbuff);
  }

  mkc_log (process->log, MKC_LOG_CHECK, "== end internal checks\n");

  return process;
}

void
mkc_process_free (mkc_process_t *process)
{
  if (process == NULL) {
    return;
  }

  if (process->check != NULL) {
    mkc_check_free (process->check);
  }
  datafree (process->projectname);

  mkc_process_attr_clear (process);
  mkc_list_free (process->attr.alternates);
  mkc_list_free (process->attr.pathlist);
  mkc_list_free (process->attr.replacelist);
  mkc_list_free (process->attr.sourcelist);

  if (process->rxshellvar != NULL) {
#if _have_regex
    mkc_regex_free (process->rxshellvar);
#endif
  }
  if (process->rxincguard != NULL) {
#if _have_regex
    mkc_regex_free (process->rxincguard);
#endif
  }
  mkc_list_free (process->user_rx_list);
  free (process);
}

int32_t
mkc_process_condition (mkc_process_t *process, value_t *value)
{
  int32_t   rval;

  if (process == NULL) {
    return 0;
  }

  rval = scopedvar_value_get_integer (process->scopedvar, value);
  return rval;
}

void
mkc_process_range_init (mkc_process_t *process,
    value_t *value, value_t *beg, value_t *end, value_t *incr)
{
  int32_t     ibeg, iend, iincr;

  ibeg = scopedvar_value_get_integer (process->scopedvar, beg);
  iend = scopedvar_value_get_integer (process->scopedvar, end);
  iincr = scopedvar_value_get_integer (process->scopedvar, incr);
  if (mkc_error_chk_err (process->mkcerr)) {
    return;
  }
  value_range_init (value, ibeg, iend, iincr);
}

int32_t
mkc_process_num_op (mkc_process_t *process, mkc_astnode_token_t asttype,
    value_t *vala, value_t *valb)
{
  int32_t   result = 0;
  int32_t   ivala, ivalb;
  char      tbuff [MKC_VNAME_MAX];
  int       iasttype = asttype;

  if (process == NULL) {
    return 0;
  }

  mkc_log (process->log, MKC_LOG_PROCESS, "  p-num-op-a: %s\n",
      value_to_str (vala, tbuff, sizeof (tbuff)));
  mkc_log (process->log, MKC_LOG_PROCESS, "  p-num-op-b: %s\n",
      value_to_str (valb, tbuff, sizeof (tbuff)));
  ivala = scopedvar_value_get_integer (process->scopedvar, vala);
  ivalb = scopedvar_value_get_integer (process->scopedvar, valb);
  if (mkc_error_chk_err (process->mkcerr)) {
    return 0;
  }
  mkc_log (process->log, MKC_LOG_PROCESS,
      "  p-num-op: |%" PRId32 "|%" PRId32 "|\n", ivala, ivalb);

  switch (iasttype) {
    case MKC_T_OP_NUM_EQ: {
      result = (ivala == ivalb);
      break;
    }
    case MKC_T_OP_NUM_NE: {
      result = (ivala != ivalb);
      break;
    }
    case MKC_T_OP_NUM_LT: {
      result = (ivala < ivalb);
      break;
    }
    case MKC_T_OP_NUM_LE: {
      result = (ivala <= ivalb);
      break;
    }
    case MKC_T_OP_NUM_GT: {
      result = (ivala > ivalb);
      break;
    }
    case MKC_T_OP_NUM_GE: {
      result = (ivala >= ivalb);
      break;
    }
    case MKC_T_OP_PLUS: {
      result = ivala + ivalb;
      break;
    }
    case MKC_T_OP_MINUS: {
      result = ivala - ivalb;
      break;
    }
    case MKC_T_OP_MULTIPLY: {
      result = ivala * ivalb;
      break;
    }
    case MKC_T_OP_DIVIDE: {
      if (ivalb == 0) {
        mkc_error_set (process->mkcerr, MKC_ERR_DIVIDE_BY_ZERO, 0, NULL);
        break;
      }
      result = ivala / ivalb;
      break;
    }
    case MKC_T_OP_MODULO: {
      if (ivalb == 0) {
        mkc_error_set (process->mkcerr, MKC_ERR_DIVIDE_BY_ZERO, 0, NULL);
        break;
      }
      result = ivala % ivalb;
      break;
    }
    default: {
      result = 0;
      mkc_error_set (process->mkcerr, MKC_ERR_INVALID_OP, 0, NULL);
      break;
    }
  }

  return result;
}

int32_t
mkc_process_str_op (mkc_process_t *process, mkc_astnode_token_t asttype,
    value_t *vala, value_t *valb)
{
  int32_t     result = 0;
  char        stra [MKC_PATH_MAX];
  char        strb [MKC_PATH_MAX];
  int         iasttype = asttype;

  if (process == NULL) {
    return 0;
  }

  mkc_log (process->log, MKC_LOG_PROCESS, "  p-str-op-a: %s\n",
      value_to_str (vala, stra, sizeof (stra)));
  mkc_log (process->log, MKC_LOG_PROCESS, "  p-str-op-b: %s\n",
      value_to_str (valb, strb, sizeof (strb)));
  scopedvar_value_get_str (process->scopedvar, vala, stra, sizeof (stra));
  scopedvar_value_get_str (process->scopedvar, valb, strb, sizeof (strb));
  if (mkc_error_chk_err (process->mkcerr)) {
    return 0;
  }
  mkc_log (process->log, MKC_LOG_PROCESS, "  p-str-op: |%s|%s|\n",
      stra, strb);

  switch (iasttype) {
    case MKC_T_OP_STR_EQ: {
      result = strcmp (stra, strb) == 0;
      break;
    }
    case MKC_T_OP_STR_NE: {
      result = strcmp (stra, strb) != 0;
      break;
    }
    case MKC_T_OP_STR_LT: {
      result = strcmp (stra, strb) < 0;
      break;
    }
    case MKC_T_OP_STR_LE: {
      result = strcmp (stra, strb) <= 0;
      break;
    }
    case MKC_T_OP_STR_GT: {
      result = strcmp (stra, strb) > 0;
      break;
    }
    case MKC_T_OP_STR_GE: {
      result = strcmp (stra, strb) >= 0;
      break;
    }
    case MKC_T_OP_STR_EQ_REGEX:
    case MKC_T_OP_STR_NE_REGEX: {
      mkc_user_regex_t    *urx;

      urx = mkc_process_user_regex_init (process, strb);
      if (urx == NULL) {
        break;
      }
      result = 0;
#if _have_regex
      result = mkc_regex_match (urx->rx, stra);
      if (asttype == MKC_T_OP_STR_NE_REGEX) {
        result = ! result;
      }
#endif
      break;
    }
    default: {
      result = 0;
      mkc_error_set (process->mkcerr, MKC_ERR_INVALID_OP, 0, NULL);
      break;
    }
  }

  return result;
}

int32_t
mkc_process_unary_op (mkc_process_t *process, mkc_astnode_token_t asttype,
    value_t *vala)
{
  int32_t     result = 0;
  int32_t     ivala = 0;
  int         iasttype = asttype;

  if (process == NULL) {
    return 0;
  }

  ivala = scopedvar_value_get_integer (process->scopedvar, vala);
  if (mkc_error_chk_err (process->mkcerr)) {
    return 0;
  }

  switch (iasttype) {
    case MKC_T_OP_NOT: {
      result = ! ivala;
      break;
    }
    case MKC_T_OP_UNARY_MINUS: {
      result = - ivala;
      break;
    }
    case MKC_T_OP_UNARY_PLUS: {
      result = ivala;
      break;
    }
    default: {
      result = 0;
      mkc_error_set (process->mkcerr, MKC_ERR_INVALID_OP, 0, NULL);
      break;
    }
  }

  return result;
}

int32_t
mkc_process_other_op (mkc_process_t *process, mkc_astnode_token_t asttype,
    value_t *vala)
{
  int32_t     result = 0;
  char        *tbuff;
  int         iasttype = asttype;

  if (process == NULL) {
    return 0;
  }

  if (vala->vtype == MKC_VT_INTEGER ||
      vala->vtype == MKC_VT_RANGE ||
      vala->vtype == MKC_VT_INVALID ||
      vala->vtype == MKC_VT_LIST) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT_TYPE, 0, NULL);
    return 0;
  }

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return 0;
  }

  switch (iasttype) {
    case MKC_T_OP_FILE_EXISTS: {
      scopedvar_value_get_str (process->scopedvar, vala, tbuff, MKC_PATH_MAX);
      result = fileop_exists (tbuff);
      break;
    }
    case MKC_T_OP_IS_DEFINED: {
      scopedvar_value_get_str (process->scopedvar, vala, tbuff, MKC_PATH_MAX);
      result = scopedvar_is_defined (process->scopedvar, tbuff);
      break;
    }
    case MKC_T_OP_IS_DIRECTORY: {
      scopedvar_value_get_str (process->scopedvar, vala, tbuff, MKC_PATH_MAX);
      result = fileop_is_directory (tbuff);
      break;
    }
    case MKC_T_OP_IS_LIST: {
      scopedvar_value_get_str (process->scopedvar, vala, tbuff, MKC_PATH_MAX);
      result = scopedvar_var_is_list (process->scopedvar, tbuff);
      break;
    }
    default: {
      result = 0;
      mkc_error_set (process->mkcerr, MKC_ERR_INVALID_OP, 0, NULL);
      break;
    }
  }

  free (tbuff);
  return result;
}

void
mkc_process_include (mkc_process_t *process,
    value_t *valpath, value_t *valfn,
    char *buff, size_t sz)
{
  char      *p = buff;
  char      *tbuff;
  char      *fname;

  *p = '\0';

  if (process == NULL) {
    return;
  }
  if (valfn == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  fname = malloc (MKC_PATH_MAX);
  if (fname == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  scopedvar_value_get_str (process->scopedvar, valfn, fname, MKC_PATH_MAX);
  if (fileop_exists (fname)) {
    p = stpecpy (p, buff + sz, fname);
    free (fname);
    return;
  }

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  if (valpath != NULL) {
    scopedvar_value_get_str (process->scopedvar, valpath, tbuff, MKC_PATH_MAX);
    p = stpecpy (p, buff + sz, tbuff);
    p = stpecpy (p, buff + sz, "/");
    p = stpecpy (p, buff + sz, fname);
    if (! fileop_exists (buff)) {
      mkc_error_set (process->mkcerr, MKC_ERR_FILE_NOT_FOUND, 0, NULL);
    }
  }

  if (valpath == NULL) {
    path_build (MKC_PATH_MKC_UNITS, tbuff, MKC_PATH_MAX, fname, process->mkcerr);
    if (fileop_exists (tbuff)) {
      p = stpecpy (buff, buff + sz, tbuff);
    }
    path_build (MKC_PATH_MKC_USER_UNITS, tbuff, MKC_PATH_MAX, fname, process->mkcerr);
    if (fileop_exists (tbuff)) {
      p = stpecpy (buff, buff + sz, tbuff);
    }
  }

  free (tbuff);
  free (fname);
}

/* control statements */

mkc_foreach_t *
mkc_process_stmt_foreach_setup (mkc_process_t *process,
    value_t *valnm, value_t *vallist)
{
  mkc_foreach_t   *pforeach;

  if (process == NULL) {
    return NULL;
  }
  if (valnm == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

  pforeach = malloc (sizeof (mkc_foreach_t));
  if (pforeach == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  scopedvar_push (process->scopedvar, SV_T_LOCAL, "local-foreach");
  pforeach->namelist = NULL;
  pforeach->listval = NULL;
  pforeach->iteridx = MKC_ITER_FINISH;
  value_init (&pforeach->tvalue);

  if (valnm != NULL) {
    value_t   *value;

    if (valnm->vtype == MKC_VT_RANGE) {
      mkc_error_set (process->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT_TYPE, 0, NULL);
      return NULL;
    }

    value = scopedvar_value_get_list_value (process->scopedvar, valnm);
    pforeach->namelist = value->list;
  }
  if (vallist != NULL) {
    value_t   *value;

    value = vallist;
    pforeach->listval = scopedvar_value_get_list_value (process->scopedvar, vallist);
    value_iter_start (value, &pforeach->iteridx);
  }

  return pforeach;
}

bool
mkc_process_stmt_foreach (mkc_process_t *process, mkc_foreach_t *pforeach)
{
  mkc_listidx_t   niteridx;
  mkc_listidx_t   nidx;
  bool            cont = true;

  mkc_list_iter_start (pforeach->namelist, &niteridx);
  while ((nidx = mkc_list_iter_next (pforeach->namelist, &niteridx)) != MKC_ITER_FINISH) {
    value_t     *nval = NULL;
    mkc_listidx_t   rc;

    nval = mkc_list_get_by_idx (pforeach->namelist, nidx);

    rc = value_iter_next (pforeach->listval, &pforeach->tvalue, &pforeach->iteridx);
    if (rc == MKC_ITER_FINISH) {
      cont = false;
      break;
    }
    mkc_process_local_set (process, nval, &pforeach->tvalue);
  }

  return cont;
}

void
mkc_process_stmt_foreach_finish (mkc_process_t *process, mkc_foreach_t *pforeach)
{
  scopedvar_pop (process->scopedvar);
  free (pforeach);
}

/* statements */

int
mkc_process_stmt_chk_inc_compile (mkc_process_t *process)
{
  int             rc = MKC_ERR_FAILURE;
#if _have_regex
  mkc_list_t      * hlist = NULL;
  mkc_regex_t     * rx;
  mkc_listidx_t   hiteridx;
  char            * hdrpath;
  const char      * hdr;
  char            ** cflags = NULL;
  time_t          ts;
  int             count = 0;

  if (process->attr.str [MKC_ATTR_MATCH] == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "match");
    mkc_process_attr_clear (process);
    return rc;
  }

  if (mkc_list_size (process->attr.pathlist) == 0) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "path");
    mkc_process_attr_clear (process);
    return rc;
  }

  hdrpath = malloc (MKC_PATH_MAX);
  if (hdrpath == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    mkc_process_attr_clear (process);
    return rc;
  }

  rx = mkc_regex_init (process->attr.str [MKC_ATTR_MATCH],
      MKC_REGEX_NONE, process->mkcerr);
  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_process_attr_clear (process);
    free (hdrpath);
    return rc;
  }

  process->attr.localheader = true;
  process->attr.printerrors = true;
  cflags = mkc_process_get_flags (process, MKC_C_CFLAGS);

  ts = 0;
  if (scopedvar_is_defined (process->scopedvar, MKC_C_CHK_INC_COMPILE_TS)) {
    ts = scopedvar_get_timestamp (process->scopedvar, MKC_C_CHK_INC_COMPILE_TS);
  }
  hlist = mkc_process_get_include_list (process, rx, &ts);

  mkc_list_iter_start (hlist, &hiteridx);
  while ((hdr = mkc_process_iter_includes (process, hlist,
      &hiteridx, hdrpath, MKC_PATH_MAX)) != NULL) {
    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    count += 1;
    rc = mkc_chk_header (process->check, process->attr.currcompiler, hdr,
        (const char **) cflags);
    if (rc != MKC_OK) {
      mkc_error_set (process->mkcerr, MKC_ERR_INCLUDE_COMPILE_FAIL, 0, hdrpath);
      break;
    }
  }

  if (count == 0) {
    mkc_message ("-- cached: check_include_compile\n");
    mkc_log (process->log, MKC_LOG_CHECK, "-- cached: check_include_compile\n");
  } else {
    ts = mstime ();
    scopedvar_set_timestamp (process->scopedvar, SV_T_INTERNAL,
        MKC_C_CHK_INC_COMPILE_TS, ts, MKC_VCTXT_MKC);

    mkc_message ("-- check_include_compile - %s (%d)\n",
        mkc_success_msg (rc), count);
    mkc_log (process->log, MKC_LOG_CHECK, "-- check_include_compile - %s (%d)\n",
        mkc_success_msg (rc), count);
  }

  mkc_flags_free (cflags);
  mkc_list_free (hlist);
  mkc_regex_free (rx);
  free (hdrpath);
#endif
  mkc_process_attr_clear (process);
  return rc;
}

int
mkc_process_stmt_chk_inc_deps (mkc_process_t *process)
{
  mkc_list_t      * hlist = NULL;
  toposort_t      * topo = NULL;
  mkc_regex_t     * rx = NULL;
  mkc_listidx_t   hiteridx;
  int             rc = MKC_ERR_FAILURE;
  char            * rbuff = NULL;
  char            * hdrpath = NULL;
  const char      * hdr;
  time_t          ts;

  if (process->attr.str [MKC_ATTR_MATCH] == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "match");
    mkc_process_attr_clear (process);
    return rc;
  }

  if (mkc_list_size (process->attr.pathlist) == 0) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "path");
    mkc_process_attr_clear (process);
    return rc;
  }

  mkc_log (process->log, MKC_LOG_CHECK, "== chk-include-deps\n");

#if _have_regex
  rx = mkc_regex_init (process->attr.str [MKC_ATTR_MATCH],
      MKC_REGEX_NONE, process->mkcerr);
#endif
  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_process_attr_clear (process);
    return rc;
  }

  /* always select all include files */
  /* the returned timestamp will be used to determine */
  /* if a check needs to be made */
  ts = 0;
  hlist = mkc_process_get_include_list (process, rx, &ts);
  /* ts now holds the timestamp of the latest modification time in ms */

  if (scopedvar_is_defined (process->scopedvar, MKC_C_CHK_INC_DEPS_TS)) {
    time_t        cachedts;

    cachedts = scopedvar_get_timestamp (process->scopedvar, MKC_C_CHK_INC_DEPS_TS);

    if (cachedts > ts) {
      mkc_message ("-- cached: check_include_dependencies\n");
      mkc_log (process->log, MKC_LOG_CHECK, "-- cached: check_include_dependencies\n");

      mkc_list_free (hlist);
      mkc_process_attr_clear (process);
#if _have_regex
      mkc_regex_free (rx);
#endif
      return rc;
    }
  }

  hdrpath = malloc (MKC_PATH_MAX);
  if (hdrpath == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    mkc_process_attr_clear (process);
#if _have_regex
    mkc_regex_free (rx);
#endif
    return rc;
  }

  topo = toposort_init (process->mkcerr);
  mkc_process_topo_add_items (process, topo, hlist);

// ### check the timestamp, see if it is necessary to process the deps.
// need to re-work process-iter-includes...

  mkc_list_iter_start (hlist, &hiteridx);
  while ((hdr = mkc_process_iter_includes (process, hlist, &hiteridx,
      hdrpath, MKC_PATH_MAX)) != NULL) {
    size_t      fsz = 0;

    rbuff = fileop_read_file (hdrpath, &fsz, process->mkcerr);
    mkc_process_topo_add_deps (process, topo, rbuff, hdr);
    free (rbuff);
  }
  mkc_list_free (hlist);

  rc = toposort (topo);
  if (rc == MKC_ERR_FAILURE) {
    char    tbuff [MKC_VNAME_MAX];

    toposort_disp_cycle (topo, tbuff, sizeof (tbuff));
    mkc_error_set (process->mkcerr, MKC_ERR_DEPENDENCY_CYCLE, 0, tbuff);
  }

  ts = mstime ();
  scopedvar_set_timestamp (process->scopedvar, SV_T_INTERNAL,
      MKC_C_CHK_INC_DEPS_TS, ts, MKC_VCTXT_MKC);

  mkc_message ("-- check_include_dependencies - %s\n", mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check_include_dependencies - %s\n",
      mkc_success_msg (rc));

#if _have_regex
  mkc_regex_free (rx);
#endif
  toposort_free (topo);
  free (hdrpath);
  mkc_process_attr_clear (process);
  return rc;
}

int
mkc_process_stmt_chk_inc_guards (mkc_process_t *process)
{
  int             rc = MKC_ERR_FAILURE;
#if _have_regex
  mkc_list_t      * hlist = NULL;
  mkc_regex_t     * rx;
  mkc_listidx_t   hiteridx;
  char            * rbuff;
  char            * hdrpath;
  const char      * hdr;
  char            ** match = NULL;
  int             matchcount;
  mkc_list_t      * guardlist = NULL;
  time_t          ts;
  int             count = 0;

  if (process->attr.str [MKC_ATTR_MATCH] == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "match");
    mkc_process_attr_clear (process);
    return rc;
  }

  if (mkc_list_size (process->attr.pathlist) == 0) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "path");
    mkc_process_attr_clear (process);
    return rc;
  }

  guardlist = mkc_list_init (MKC_LIST_SORTED, mkc_list_ind_free,
      mkc_list_ind_compare, process->mkcerr);

  if (process->rxincguard == NULL) {
    process->rxincguard = mkc_regex_init (
        "^# *ifndef +([[:alnum:]_][[:alnum:]_]*)[\r\n]+# *define +\\g1[\r]*$",
        MKC_REGEX_MULTILINE, process->mkcerr);
  }

  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_process_attr_clear (process);
    return rc;
  }

  rx = mkc_regex_init (process->attr.str [MKC_ATTR_MATCH],
      MKC_REGEX_NONE, process->mkcerr);
  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_process_attr_clear (process);
    return rc;
  }

  rc = MKC_OK;
  ts = 0;
  /* always select all include files */
  /* as chk-inc-guards compares all guards to check for duplicates */
  /* the returned timestamp will be used to determine */
  /* if a check needs to be made */

  hlist = mkc_process_get_include_list (process, rx, &ts);

  if (scopedvar_is_defined (process->scopedvar, MKC_C_CHK_INC_GUARDS_TS)) {
    time_t    cachedts;

    cachedts = scopedvar_get_timestamp (process->scopedvar, MKC_C_CHK_INC_GUARDS_TS);

    if (cachedts > ts) {
      mkc_message ("-- cached: check_include_guards\n");
      mkc_log (process->log, MKC_LOG_CHECK, "-- cached: check_include_guards\n");

      mkc_list_free (hlist);
      mkc_process_attr_clear (process);
#if _have_regex
      mkc_regex_free (rx);
#endif
      return rc;
    }
  }

  hdrpath = malloc (MKC_PATH_MAX);
  if (hdrpath == NULL) {
    mkc_regex_free (rx);
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    mkc_process_attr_clear (process);
    return rc;
  }

  mkc_list_iter_start (hlist, &hiteridx);
  while ((hdr = mkc_process_iter_includes (process, hlist, &hiteridx,
      hdrpath, MKC_PATH_MAX)) != NULL) {
    char            *tp;
    mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
    mkc_listidx_t   idx;
    size_t          fsz = 0;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    rbuff = fileop_read_file (hdrpath, &fsz, process->mkcerr);
    count += 1;
    mkc_regex_get_reset (process->rxincguard);
    match = mkc_regex_get (process->rxincguard, rbuff, &matchcount);
    if (matchcount != 2) {
      mkc_error_set (process->mkcerr, MKC_ERR_INCLUDE_GUARD_NOTFOUND, 0, hdrpath);
      rc = MKC_ERR_FAILURE;
    }

    if (matchcount == 2) {
      tp = strdup (match [1]);
      loc = MKC_LIST_NOTFOUND;
      idx = mkc_list_find (guardlist, &tp, &loc);
      if (idx != MKC_LIST_NOTFOUND) {
        mkc_error_set (process->mkcerr, MKC_ERR_INCLUDE_GUARD_DUPLICATE, 0, hdrpath);
        rc = MKC_ERR_FAILURE;
      }

      mkc_list_set (guardlist, &tp, sizeof (char *), &loc);
    }
    mkc_regex_get_free (match);
    free (rbuff);
  }

  ts = mstime ();
  scopedvar_set_timestamp (process->scopedvar, SV_T_INTERNAL,
      MKC_C_CHK_INC_GUARDS_TS, ts, MKC_VCTXT_MKC);

  mkc_message ("-- check_include_guards - %s (%d)\n",
      mkc_success_msg (rc), count);
  mkc_log (process->log, MKC_LOG_CHECK, "-- check_include_guards - %s (%d)\n",
      mkc_success_msg (rc), count);

  mkc_list_free (hlist);
  mkc_list_free (guardlist);
  mkc_regex_free (rx);
  free (hdrpath);
#endif
  mkc_process_attr_clear (process);
  return rc;
}

void
mkc_process_stmt_configure (mkc_process_t *process)
{
  int       defzero = MKC_AUTO_SKIP_ZERO;

  if (process->attr.str [MKC_ATTR_METHOD] == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "method");
    mkc_process_attr_clear (process);
    return;
  }

  defzero = process->attr.define_zero;

  if (strcmp (process->attr.str [MKC_ATTR_METHOD], "auto") == 0) {
    mkc_process_configure_auto (process, defzero);
  } else if (strcmp (process->attr.str [MKC_ATTR_METHOD], "manual") == 0) {
    if (process->attr.str [MKC_ATTR_INPUT] == NULL) {
      mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "input");
      mkc_process_attr_clear (process);
      return;
    }
    if (process->attr.str [MKC_ATTR_OUTPUT] == NULL) {
      mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "output");
      mkc_process_attr_clear (process);
      return;
    }
    mkc_process_configure_manual (process);
  } else {
    mkc_error_set (process->mkcerr, MKC_ERR_PROC_INVALID_METHOD, 0, NULL);
  }

  mkc_process_attr_clear (process);
  return;
}

int
mkc_process_stmt_debug (mkc_process_t *process,
    value_t *value, value_t *subvalue)
{
  char    tbuff [MKC_VNAME_MAX];

  scopedvar_value_get_str (process->scopedvar, value, tbuff, sizeof (tbuff));

  if (strcmp (tbuff, "null") == 0) {
    /* do nothing */ ;
  }
  if (strcmp (tbuff, "printprof") == 0) {
    mkc_process_dbg_print_prof (process);
  }
  if (strcmp (tbuff, "printvar") == 0) {
    scopedvar_value_get_str (process->scopedvar, subvalue, tbuff, sizeof (tbuff));
    mkc_process_dbg_print_var (process, tbuff);
  }
  if (strcmp (tbuff, "printpath") == 0) {
    mkc_process_dbg_print_path (process);
  }
  if (strcmp (tbuff, "printinternal") == 0) {
    mkc_process_dbg_print_int_var (process);
  }

  return false;
}

void
mkc_process_stmt_executable (mkc_process_t *process, value_t *valnm)
{
  char            nm [MKC_VNAME_MAX];
  char            execnm [MKC_VNAME_MAX];
  mkc_listidx_t   siteridx;
  mkc_listidx_t   sidx;
  char            *tbuff;


  scopedvar_value_get_str (process->scopedvar, valnm, nm, sizeof (nm));
  snprintf (execnm, sizeof (execnm), "%s%s", nm, process->exeext);

  scopedvar_push (process->scopedvar, SV_T_TARGET, execnm);

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  mkc_list_iter_start (process->attr.sourcelist, &siteridx);
  while ((sidx = mkc_list_iter_next (process->attr.sourcelist, &siteridx)) != MKC_ITER_FINISH) {
    value_t     *src;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    src = mkc_list_get_by_idx (process->attr.sourcelist, sidx);
    scopedvar_value_get_str (process->scopedvar, src, tbuff, MKC_PATH_MAX);
    mkc_process_source_file (process, execnm, tbuff);
  }

  free (tbuff);
  return;
}

void
mkc_process_stmt_function_call (mkc_process_t *process,
    value_t *valparams, value_t *valfuncargs)
{
  mkc_list_t      *paramlist = NULL;
  mkc_list_t      *alist = NULL;
  mkc_listidx_t   aiteridx;
  mkc_listidx_t   nmiteridx;
  mkc_listidx_t   aidx;
  mkc_listidx_t   nmidx;

  scopedvar_push (process->scopedvar, SV_T_LOCAL, "local-function");

  if (valparams != NULL) {
    value_t   *value;

    value = scopedvar_value_get_list_value (process->scopedvar, valparams);
    if (value->vtype == MKC_VT_RANGE) {
      mkc_error_set (process->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT_TYPE, 0, NULL);
      return;
    }
    paramlist = value->list;
  }
  if (valfuncargs != NULL) {
    value_t   *value = NULL;
    value = scopedvar_value_get_list_value (process->scopedvar, valfuncargs);
    alist = value->list;
  }
  if ((alist == NULL && paramlist != NULL) ||
      (alist != NULL && paramlist == NULL) ||
      (alist != NULL &&
          mkc_list_size (alist) != mkc_list_size (paramlist))) {
    mkc_error_set (process->mkcerr, MKC_ERR_FUNCTION_ARG_MISMATCH, 0, NULL);
    return;
  }

  /* put the arguments into the local profile */
  mkc_list_iter_start (alist, &aiteridx);
  mkc_list_iter_start (paramlist, &nmiteridx);
  while ((aidx = mkc_list_iter_next (alist, &aiteridx)) != MKC_ITER_FINISH) {
    value_t     *aval;
    value_t     *nmval;

    nmidx = mkc_list_iter_next (paramlist, &nmiteridx);

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    aval = mkc_list_get_by_idx (alist, aidx);
    nmval = mkc_list_get_by_idx (paramlist, nmidx);
    mkc_process_local_set (process, nmval, aval);
  }
}

void
mkc_process_stmt_function_call_finish (mkc_process_t *process)
{
  scopedvar_pop (process->scopedvar);
}

void
mkc_process_stmt_loadcache (mkc_process_t *process, value_t *valvers)
{
  int     version;

  version = scopedvar_value_get_integer (process->scopedvar, valvers);
  if (version != 1) {
    mkc_message ("-- cache version mismatch\n");
    mkc_process_attr_clear (process);
    return;
  }

  process->inloadcache = true;
  scopedvar_set_fromcache (process->scopedvar, true);

  mkc_process_attr_clear (process);
  return;
}

void
mkc_process_stmt_loadcache_post (mkc_process_t *process)
{
  process->cacheloaded = true;
  scopedvar_set_fromcache (process->scopedvar, false);

  if (process->cacheloaded && process->cacheinvalidated) {
    scopedvar_reset (process->scopedvar, process->mkcoptions);

    mkc_message ("-- cache invalidated\n");
    mkc_log (process->log, MKC_LOG_GENERAL, "-- cache invalidated\n");
    mkc_process_set_defaults (process);
    mkc_process_int_checks (process);
    mkc_process_get_path (process);
    mkc_process_find_executables (process);
  }

  process->inloadcache = false;

  mkc_process_attr_clear (process);
  return;
}

void
mkc_process_stmt_mark (mkc_process_t *process,
    value_t *vala, value_t *valb)
{
  char    nm [MKC_VNAME_MAX];
  char    val [MKC_VNAME_MAX];

  if (process == NULL) {
    return;
  }

  scopedvar_value_get_str (process->scopedvar, vala, nm, sizeof (nm));
  scopedvar_value_get_str (process->scopedvar, valb, val, sizeof (val));
  if (*nm == '\0') {
    mkc_error_set (process->mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
    mkc_process_attr_clear (process);
    return;
  }
  if (strcmp (val, "disable-output") == 0 ||
      strcmp (val, "disable") == 0) {
    scopedvar_set_context (process->scopedvar, nm, MKC_VCTXT_USER_DISABLE);
  } else if (strcmp (val, "enable-output") == 0 ||
      strcmp (val, "enable") == 0) {
    scopedvar_set_context (process->scopedvar, nm, MKC_VCTXT_USER_ENABLE);
  } else {
    mkc_error_set (process->mkcerr, MKC_ERR_PROC_INVALID_MARK, 0, NULL);
  }

  mkc_process_attr_clear (process);
  return;
}

void
mkc_process_stmt_print (mkc_process_t *process, value_t *value, int depth)
{
  char      tbuff [MKC_PATH_MAX];

  if (process == NULL) {
    return;
  }
  if (value == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  scopedvar_value_get_str (process->scopedvar, value, tbuff, sizeof (tbuff));
  fprintf (stdout, "%s", tbuff);

  if (depth == 0) {
    fprintf (stdout, "\n");
    fflush (stdout);
  }
}

void
mkc_process_stmt_profile (mkc_process_t *process, value_t *valnm)
{
  char        nm [MKC_VNAME_MAX];

  scopedvar_value_get_str (process->scopedvar, valnm, nm, sizeof (nm));

  scopedvar_set_active_profile (process->scopedvar, nm);
  /* if a compiler is set, it has not yet been processed */
}

void
mkc_process_stmt_profile_post (mkc_process_t *process)
{
  scopedvar_reset_profile (process->scopedvar);
}

void
mkc_process_stmt_project (mkc_process_t *process)
{
  mkc_alternate_t * alt;

  alt = process->attr.curralt;
  if (alt->name == NULL || *(alt->name) == '\0') {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "name");
    mkc_process_attr_clear (process);
    return;
  }

  datafree (process->projectname);
  process->projectname = strdup (alt->name);
  process->dfltcompiler = process->attr.currcompiler;

//  mkc_pvar_profile_select_idx (process->pvar, process->pidx_internal);
  scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
      MKC_C_PROJECT_NAME, process->projectname, MKC_VCTXT_MKC);
  if (process->attr.str [MKC_ATTR_VERSION] != NULL) {
    scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
        MKC_C_PROJECT_VERS, process->attr.str [MKC_ATTR_VERSION], MKC_VCTXT_MKC);
  }
  if (process->attr.str [MKC_ATTR_LIB_VERSION] != NULL) {
    scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
        MKC_C_PROJECT_LIB_VERS, process->attr.str [MKC_ATTR_LIB_VERSION], MKC_VCTXT_MKC);
  }
//  mkc_pvar_profile_select_idx (process->pvar, pidx);

  mkc_process_attr_clear (process);

  return;
}

int
mkc_process_stmt_set (mkc_process_t *process,
    value_t *valnm, value_t *value, bool local)
{
  char            nm [MKC_VNAME_MAX];
  value_t         *tvalue;
  mkc_err_code_t  trc = MKC_ERR_FAILURE;
  value_ctxt_t    vctxt = MKC_VCTXT_USER_DISABLE;
  bool            istempval = false;
  sv_type_t       svtype;

  if (process == NULL) {
    return trc;
  }
  if (valnm == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    mkc_process_attr_clear (process);
    return trc;
  }

  scopedvar_value_get_str (process->scopedvar, valnm, nm, sizeof (nm));
  if (*nm == '\0') {
    mkc_error_set (process->mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
    mkc_process_attr_clear (process);
    return trc;
  }

  tvalue = scopedvar_value_get_value (process->scopedvar, value);
  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_process_attr_clear (process);
    return trc;
  }
  istempval = tvalue->tempallocated;

  if (process->attr.str [MKC_ATTR_VCONTEXT] != NULL) {
    const char    *tvc;

    tvc = process->attr.str [MKC_ATTR_VCONTEXT];
    if (strcmp (tvc, "check") == 0) {
      vctxt = MKC_VCTXT_CHECK;
    } else if (strcmp (tvc, "env") == 0) {
      vctxt = MKC_VCTXT_ENV;
    } else if (strcmp (tvc, "flag") == 0) {
      vctxt = MKC_VCTXT_FLAG;
    } else if (strcmp (tvc, "mkc") == 0) {
      vctxt = MKC_VCTXT_MKC;
    } else if (strcmp (tvc, "temp") == 0) {
      vctxt = MKC_VCTXT_TEMP;
    } else if (strcmp (tvc, "disable") == 0 ||
        strcmp (tvc, "disable-output") == 0) {
      vctxt = MKC_VCTXT_USER_DISABLE;
    } else if (strcmp (tvc, "enable") == 0 ||
        strcmp (tvc, "enable-output") == 0) {
      vctxt = MKC_VCTXT_USER_ENABLE;
    }
  }

  svtype = SV_T_SEARCH;
  if (local) {
    svtype = SV_T_LOCAL;
  }

  if (process->attr.str [MKC_ATTR_NAMESPACE] != NULL) {
    const char    *tns;

    tns = process->attr.str [MKC_ATTR_NAMESPACE];
    if (strcmp (tns, "timestamp") == 0) {
      svtype = SV_T_TIMESTAMP;
    } else if (strcmp (tns, "dependency") == 0) {
      svtype = SV_T_DEPENDENCY;
    } else if (strcmp (tns, "paths") == 0) {
      svtype = SV_T_PATHS;
    }
  }

  trc = scopedvar_set (process->scopedvar, svtype, nm, tvalue, vctxt);
  if (trc == MKC_OK_CHANGE) {
    process->cacheinvalidated = true;
  }

  /* tvalue may have been re-allocated, only call temp-value-free */
  /* if the tvalue was allocated */
  if (istempval) {
    scopedvar_temp_value_free (tvalue);
  }

  mkc_process_attr_clear (process);
  return trc;
}

/* attributes */

void
mkc_process_attribute (mkc_process_t *process, value_t *valname,
    mkc_astnode_token_t asttype)
{
  char            nm [MKC_VNAME_MAX];
  int             iasttype = asttype;
  mkc_ctxt_val_t  ctxt = 0;
  char            **p = NULL;
  mkc_attr_type_t attrtype = MKC_ATTR_MAX;

  if (process == NULL) {
    return;
  }

  switch (iasttype) {
    case MKC_T_ATTR_CONTEXT:    { attrtype = MKC_ATTR_VCONTEXT; break; }
    case MKC_T_ATTR_INPUT:      { attrtype = MKC_ATTR_INPUT; break; }
    case MKC_T_ATTR_LIBRARY_VERSION: { attrtype = MKC_ATTR_LIB_VERSION; break; }
    case MKC_T_ATTR_MATCH:      { attrtype = MKC_ATTR_MATCH; break; }
    case MKC_T_ATTR_METHOD:     { attrtype = MKC_ATTR_METHOD; break; }
    case MKC_T_ATTR_NAMESPACE:  { attrtype = MKC_ATTR_NAMESPACE; break; }
    case MKC_T_ATTR_OUTPUT:     { attrtype = MKC_ATTR_OUTPUT; break; }
    case MKC_T_ATTR_VERSION:    { attrtype = MKC_ATTR_VERSION; break; }
  }

  switch (iasttype) {
    case MKC_T_ATTR_CONTEXT:
    case MKC_T_ATTR_INPUT:
    case MKC_T_ATTR_LIBRARY_VERSION:
    case MKC_T_ATTR_MATCH:
    case MKC_T_ATTR_METHOD:
    case MKC_T_ATTR_NAMESPACE:
    case MKC_T_ATTR_OUTPUT:
    case MKC_T_ATTR_VERSION: {
      ctxt = attrcontext [attrtype];
      break;
    }
    case MKC_T_ATTR_DEFINE_ZERO: {
      ctxt = MKC_CONTEXT_CONFIGURE;
      break;
    }
    case MKC_T_ATTR_NAME: {
      ctxt = MKC_CONTEXT_CHECK | MKC_CONTEXT_COMP_FLAG |
          MKC_CONTEXT_PROJECT | MKC_CONTEXT_ALTERNATE | MKC_CONTEXT_EXECUTABLE;
      break;
    }
    case MKC_T_ATTR_NEGATE: {
      ctxt = MKC_CONTEXT_COMP_FLAG;
      break;
    }
    default: {
      mkc_error_set (process->mkcerr, MKC_ERR_UNHANDLED_VALUE, 0, NULL);
      fprintf (stderr, "ERR: process: unhandled attr %s\n", typenames [asttype]);
      break;
    }
  }

  if (! mkc_context_check (process->context, ctxt)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  *nm = '\0';
  if (valname != NULL) {
    scopedvar_value_get_str (process->scopedvar, valname, nm, sizeof (nm));
  }

  switch (iasttype) {
    case MKC_T_ATTR_CONTEXT:
    case MKC_T_ATTR_INPUT:
    case MKC_T_ATTR_LIBRARY_VERSION:
    case MKC_T_ATTR_MATCH:
    case MKC_T_ATTR_METHOD:
    case MKC_T_ATTR_NAMESPACE:
    case MKC_T_ATTR_OUTPUT:
    case MKC_T_ATTR_VERSION: {
      p = &process->attr.str [attrtype];
      break;
    }
    case MKC_T_ATTR_DEFINE_ZERO: {
      p = NULL;
      process->attr.define_zero = MKC_AUTO_DEFINE_ZERO;
      break;
    }
    case MKC_T_ATTR_NAME: {
      mkc_alternate_t   *alt;

      alt = process->attr.curralt;
      p = &alt->name;
      break;
    }
    case MKC_T_ATTR_NEGATE: {
      p = NULL;
      process->attr.negate = true;
      break;
    }
    default: {
      mkc_error_set (process->mkcerr, MKC_ERR_UNHANDLED_VALUE, 0, NULL);
      fprintf (stderr, "ERR: process: unhandled attr %s\n", typenames [asttype]);
      break;
    }
  }

  if (p != NULL) {
    datafree (*p);
    *p = strdup (nm);
    if (*p == NULL) {
      mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    }
  }
}

void
mkc_process_attr_alternate (mkc_process_t *process)
{
  mkc_alternate_t   alt;
  mkc_listidx_t     loc = MKC_LIST_NOTFOUND;

  alt.name = NULL;
  alt.hdrlist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, process->mkcerr);
  alt.compflags = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, process->mkcerr);
  alt.linkflags = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, process->mkcerr);
  process->attr.curralt = mkc_list_set (process->attr.alternates,
      &alt, sizeof (mkc_alternate_t), &loc);
}

void
mkc_process_attr_compiler (mkc_process_t *process, value_t *name)
{
  char            nm [MKC_VNAME_MAX];

  if (process == NULL) {
    return;
  }

  /* the compiler attribute is only allowed in */
  /* project, profile and check-include statements */
  if (! mkc_context_check (process->context,
      MKC_CONTEXT_PROJECT | MKC_CONTEXT_PROFILE | MKC_CONTEXT_CHK_INC)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  scopedvar_value_get_str (process->scopedvar, name, nm, sizeof (nm));
  if (mkc_context_check (process->context, MKC_CONTEXT_PROJECT)) {
    /* if in a project statement, the default compiler is set */
    process->dfltcompiler = compiler_get_id (nm);
    scopedvar_set_default_compiler (process->scopedvar, process->dfltcompiler);
  }

  process->attr.currcompiler = compiler_get_id (nm);

  if (mkc_context_check (process->context, MKC_CONTEXT_PROFILE)) {
    const char  *profnm;

    scopedvar_set_current_compiler (process->scopedvar, process->attr.currcompiler);
    profnm = scopedvar_get_current_profile (process->scopedvar);
    scopedvar_set_current_profile (process->scopedvar, profnm, process->attr.currcompiler);
  }
}

void
mkc_process_attr_comp_flags (mkc_process_t *process, value_t *value)
{
  mkc_list_t      * clist;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context,
      MKC_CONTEXT_CHECK | MKC_CONTEXT_ALTERNATE | MKC_CONTEXT_CHK_INC |
      MKC_CONTEXT_EXECUTABLE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  clist = process->attr.curralt->compflags;
  mkc_process_attr_flags (process, value, clist, false);
}

void
mkc_process_attr_header (mkc_process_t *process, value_t *value)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;
  mkc_list_t      * hlist;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context,
      MKC_CONTEXT_CHECK | MKC_CONTEXT_ALTERNATE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  hlist = process->attr.curralt->hdrlist;

  mkc_list_iter_start (value->list, &iteridx);
  while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    value_t     *lvalue;
    mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    mkc_list_set (hlist, lvalue, sizeof (value_t), &loc);
  }

  return;
}

void
mkc_process_attr_link_flags (mkc_process_t *process, value_t *value)
{
  mkc_list_t      * llist;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context,
      MKC_CONTEXT_CHECK | MKC_CONTEXT_ALTERNATE | MKC_CONTEXT_EXECUTABLE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  llist = process->attr.curralt->linkflags;
  mkc_process_attr_flags (process, value, llist, false);
}

void
mkc_process_attr_path (mkc_process_t *process, value_t *path)
{
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context,
      MKC_CONTEXT_CHECK | MKC_CONTEXT_CHK_INC)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  mkc_list_set (process->attr.pathlist, path, sizeof (value_t), &loc);
  return;
}

void
mkc_process_attr_replace (mkc_process_t *process,
    value_t *str, value_t *name)
{
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context, MKC_CONTEXT_CONFIGURE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  mkc_list_set (process->attr.replacelist, str, sizeof (value_t), &loc);
  mkc_list_set (process->attr.replacelist, name, sizeof (value_t), &loc);
  return;
}

void
mkc_process_attr_source (mkc_process_t *process, value_t *value)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;
  mkc_list_t      * srclist;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context, MKC_CONTEXT_EXECUTABLE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  srclist = process->attr.sourcelist;

  mkc_list_iter_start (value->list, &iteridx);
  while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    value_t     *lvalue;
    mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    mkc_list_set (srclist, lvalue, sizeof (value_t), &loc);
  }

  return;
}

int32_t
mkc_process_check (mkc_process_t *process, value_t *valconst,
    mkc_astnode_token_t asttype)
{
  int         rc = MKC_OK;
  char        tnm [MKC_VNAME_MAX];
  char        txt [MKC_VNAME_MAX];
  char        pfx [MKC_VNAME_MAX];
  scopedvar_t     *scope;
  int         iasttype = asttype;
  bool        successtype = false;
  bool        valtype = false;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  scope = process->scopedvar;
  scopedvar_value_get_str (scope, valconst, txt, sizeof (txt));
  snprintf (pfx, sizeof (pfx), "_%s_", typenames [asttype]);
  mkc_process_create_name (process, asttype, tnm, sizeof (tnm), pfx, txt, NULL);

  if (mkc_process_chk_cache (process, txt, tnm)) {
    mkc_process_attr_clear (process);
    return rc;
  }

  switch (iasttype) {
    case MKC_T_CHK_ARG_COUNT: {
      valtype = true;
      rc = mkc_chk_arg_count (process->check, process->attr.currcompiler, txt);
      break;
    }
    case MKC_T_CHK_CONST: {
      successtype = true;
      rc = mkc_chk_const (process->check, process->attr.currcompiler, txt);
      break;
    }
    case MKC_T_CHK_DEFINE: {
      successtype = true;
      rc = mkc_chk_define (process->check, process->attr.currcompiler, txt);
      break;
    }
    case MKC_T_CHK_FUNCTION: {
      successtype = true;
      rc = mkc_chk_function (process->check, process->attr.currcompiler, txt);
      break;
    }
    case MKC_T_CHK_HEADER: {
      successtype = true;
      rc = mkc_chk_header (process->check, process->attr.currcompiler, txt, NULL);
      break;
    }
    case MKC_T_CHK_PACKAGE: {
      successtype = true;
      rc = mkc_chk_package (process->check, process->attr.currcompiler, txt);
      break;
    }
    case MKC_T_CHK_SIZE: {
      valtype = true;
      rc = mkc_chk_size (process->check, process->attr.currcompiler, txt);
      break;
    }
    case MKC_T_CHK_TYPE: {
      successtype = true;
      rc = mkc_chk_type (process->check, process->attr.currcompiler, txt);
      break;
    }
    default: {
      mkc_error_set (process->mkcerr, MKC_ERR_UNHANDLED_VALUE, 0, NULL);
      fprintf (stderr, "ERR: process: unhandled check %s\n", typenames [asttype]);
      break;
    }
  }

  if (successtype) {
    /* the check returns 0 on success */
    /* convert this to a boolean */

    scopedvar_set_integer (scope, SV_T_SEARCH, tnm, rc == 0 ? true : false, MKC_VCTXT_CHECK);
    mkc_message ("-- check %s: %s : %s - %s\n",
        typenames [asttype], txt, tnm, mkc_success_msg (rc));
    mkc_log (process->log, MKC_LOG_CHECK, "-- check %s: %s : %s - %s\n",
        typenames [asttype], txt, tnm, mkc_success_msg (rc));
  }
  if (valtype) {
    /* the check is run, and the return code is a value */
    scopedvar_set_integer (scope, SV_T_SEARCH, tnm, rc, MKC_VCTXT_CHECK);
    mkc_message ("-- check %s: %s : %s : %d\n", typenames [asttype], txt, tnm, rc);
    mkc_log (process->log, MKC_LOG_CHECK,
        "-- check %s: %s : %s : %d\n", typenames [asttype], txt, tnm, rc);
  }

  mkc_process_attr_clear (process);
  return rc;
}

int32_t
mkc_process_check_flag (mkc_process_t *process,
    value_t *valflag, int addchk, mkc_astnode_token_t asttype)
{
  int         rc = MKC_OK;
  char        tnm [MKC_VNAME_MAX];
  char        flag [MKC_VNAME_MAX];
  const char  *pfx = NULL;
  scopedvar_t     *scope;
  int         iasttype = asttype;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  scope = process->scopedvar;
  scopedvar_value_get_str (scope, valflag, flag, sizeof (flag));

  if (! *flag) {
    /* empty flags are ignored */
    return MKC_ERR_FAILURE;
  }

  switch (iasttype) {
    case MKC_T_CHK_COMP_FLAG: { pfx = "cf_"; break; }
    case MKC_T_CHK_LINK_FLAG: { pfx = "lf_"; break; }
  }
  mkc_process_create_name (process, asttype, tnm, sizeof (tnm), pfx, flag, NULL);

  if (mkc_process_chk_cache (process, flag, tnm)) {
    mkc_process_attr_clear (process);
    return rc;
  }

  if (addchk == MKC_CHK) {
    switch (iasttype) {
      case MKC_T_CHK_COMP_FLAG: {
        rc = mkc_chk_compiler_flag (process->check,
            process->attr.currcompiler, flag, process->attr.negate);
        break;
      }
      case MKC_T_CHK_LINK_FLAG: {
        rc = mkc_chk_link_flag (process->check,
            process->attr.currcompiler, flag);
        break;
      }
    }
  }
  process->attr.negate = false;

  if (rc == 0) {
    scopedvar_set_str (scope, SV_T_SEARCH, tnm, flag, MKC_VCTXT_FLAG);

    switch (iasttype) {
      case MKC_T_CHK_COMP_FLAG: {
        scopedvar_append_str_list (process->scopedvar, SV_T_ACTIVE,
            MKC_C_CFLAGS, flag, MKC_VCTXT_MKC);
        break;
      }
      case MKC_T_CHK_LINK_FLAG: {
        const char    *nm = MKC_C_LDFLAGS;

        if (mkc_flag_is_libloc (flag) ||
            strncmp (flag, "-l", 2) == 0) {
          nm = MKC_C_LIBS;
        }
        scopedvar_append_str_list (process->scopedvar, SV_T_ACTIVE,
            nm, flag, MKC_VCTXT_MKC);
        break;
      }
    }
  }

  if (addchk == MKC_ADD) {
    mkc_message ("-- add %s: %s\n", typenames [asttype], flag);
    mkc_log (process->log, MKC_LOG_CHECK,
        "-- add %s: %s\n", typenames [asttype], flag);
  }
  if (addchk == MKC_CHK) {
    mkc_message ("-- check %s: %s - %s\n",
        typenames [asttype], flag, mkc_success_msg (rc));
    mkc_log (process->log, MKC_LOG_CHECK, "-- check %s: %s - %s\n",
        typenames [asttype], flag, mkc_success_msg (rc));
  }

  mkc_process_attr_clear (process);
  return rc;
}

int32_t
mkc_process_chk_struct_member (mkc_process_t *process,
    value_t *valstructnm, value_t *valmembernm)
{
  int         rc = MKC_OK;
  char        tnm [MKC_VNAME_MAX];
  char        structname [MKC_VNAME_MAX];
  char        membername [MKC_VNAME_MAX];
  scopedvar_t     *scope;
  char        tmpdisp [MKC_VNAME_MAX * 2];

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  scope = process->scopedvar;
  scopedvar_value_get_str (scope, valstructnm, structname, sizeof (structname));
  scopedvar_value_get_str (scope, valmembernm, membername, sizeof (membername));
  mkc_process_create_name (process, MKC_T_CHK_STRUCT_MEMBER, tnm, sizeof (tnm),
      "_member_", structname, membername, NULL);

  snprintf (tmpdisp, sizeof (tmpdisp), "%s.%s", structname, membername);
  if (mkc_process_chk_cache (process, tmpdisp, tnm)) {
    mkc_process_attr_clear (process);
    return rc;
  }

  rc = mkc_chk_struct_member (process->check,
      process->attr.currcompiler, structname, membername);
  scopedvar_set_integer (scope, SV_T_SEARCH,
      tnm, rc == 0 ? true : false, MKC_VCTXT_CHECK);

  mkc_message ("-- check struct member: %s.%s - %s\n",
      structname, membername, mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check struct member: %s.%s - %s\n",
      structname, membername, mkc_success_msg (rc));

  mkc_process_attr_clear (process);
  return rc;
}

int
mkc_process_chk_shell_extract (mkc_process_t *process, value_t *valpath)
{
#if _have_regex
  char        *buff = NULL;
  size_t      fsz = 0;
  char        *path;
  char        varname [MKC_VNAME_MAX];
  char        *varvalue;
  char        **match = NULL;
  int         matchcount;
  int         rc = MKC_ERR_FAILURE;

  if (process == NULL) {
    return rc;
  }

  path = malloc (MKC_PATH_MAX);
  if (path == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return rc;
  }

  scopedvar_value_get_str (process->scopedvar, valpath, path, MKC_PATH_MAX);

  if (! fileop_exists (path)) {
    return rc;
  }

  if (process->rxshellvar == NULL) {
    process->rxshellvar = mkc_regex_init (
        "^[ \t]*([[:alnum:]_]+)=((\"(([^\"\\\\]|\\\\.)*)\")|([^ \t\r\n]*))$",
        MKC_REGEX_MULTILINE, process->mkcerr);
    /*  0: entire string */
    /*  1: var-name */
    /*  2: "..." or ... */
    /*  3: "..." */
    /*  4: ... (inside of quotes) */
    /*  5: letter (inside of quotes) */
    /*  6: ... (no quotes) */
    if (mkc_error_chk_err (process->mkcerr)) {
      return rc;
    }
  }

  varvalue = malloc (MKC_PATH_MAX);
  if (varvalue == NULL) {
    free (path);
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return rc;
  }

  buff = fileop_read_file (path, &fsz, process->mkcerr);
  if (mkc_error_chk_err (process->mkcerr)) {
    datafree (buff);
    return rc;
  }

  mkc_regex_get_reset (process->rxshellvar);
  while (true) {
    char    *tvalue;

    match = mkc_regex_get (process->rxshellvar, buff, &matchcount);
    if (matchcount != 6 && matchcount != 7) {
      mkc_regex_get_free (match);
      break;
    }

    *varname = '\0';
    *varvalue = '\0';

    mkc_log (process->log, MKC_LOG_CHECK, "  shell: matchcount: %d\n", matchcount);

    for (int i = 0; i < matchcount; ++i) {
      mkc_log (process->log, MKC_LOG_CHECK, "  shell: match: %d %s\n", i, match [i]);
    }

    stpecpy (varname, varname + sizeof (varname), match [1]);
    if (*(match [4]) != '\0') {
      /* quoted value */
      /* when matched, match [6] contains the trailing data */
      stpecpy (varvalue, varvalue + MKC_PATH_MAX, match [4]);
    } else {
      /* simple value */
      stpecpy (varvalue, varvalue + MKC_PATH_MAX, match [6]);
    }

    /* from a shell script, the user would expect escape sequences to */
    /* be substituted */
    tvalue = scopedvar_substitute (process->scopedvar, varvalue, SV_SUB_ESCAPE, 0);
    if (tvalue == NULL) {
      continue;
    }

    scopedvar_set_str (process->scopedvar, SV_T_SEARCH, varname, tvalue, MKC_VCTXT_CHECK);

    mkc_message ("-- shell extract %s %s\n", varname, tvalue);
    mkc_log (process->log, MKC_LOG_CHECK, "-- shell extract %s %s\n",
        varname, tvalue);

    mkc_regex_get_free (match);
    free (tvalue);
  }

  free (path);
  free (varvalue);
  datafree (buff);
#endif

  mkc_process_attr_clear (process);
  return MKC_OK;
}

void
mkc_process_local_set (mkc_process_t *process, value_t *nmval,
    value_t *argval)
{
  char            nm [MKC_VNAME_MAX];

  if (process == NULL) {
    return;
  }
  if (nmval == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  scopedvar_value_get_str (process->scopedvar, nmval, nm, sizeof (nm));

  scopedvar_set (process->scopedvar, SV_T_LOCAL, nm, argval, MKC_VCTXT_TEMP);
}

int32_t
mkc_process_get_loop_limit (mkc_process_t *process)
{
  int32_t   limit = 10000;
  value_t   *value;

  if (process == NULL) {
    return limit;
  }

  value = scopedvar_get_value (process->scopedvar, SV_T_INTERNAL, MKC_C_LOOPLIMIT);
  if (value != NULL) {
    limit = scopedvar_value_get_integer (process->scopedvar, value);
  }

  return limit;
}

void
mkc_process_save_cache (mkc_process_t *process)
{
  scopedvar_t     * scopedvar;
  char            * cachename;
  const char      * profname;
  FILE            * fh;
  int             tcount = 0;
  char            * tbuff;
  sv_iter_t       * sviter;

  if (mkc_error_chk_err (process->mkcerr)) {
    /* at this time, the cache is not saved if there was an error */
    return;
  }

  cachename = malloc (MKC_PATH_MAX);
  if (cachename == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  path_build (MKC_PATH_MKCFILES, cachename, MKC_PATH_MAX,
      "cache.mkc", process->mkcerr);
  fh = fileop_open (cachename, "w");
  if (fh == NULL) {
    free (cachename);
    return;
  }

  tbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  scopedvar = process->scopedvar;

  /* version 1 */
  fprintf (fh, "load_cache %d {\n", MKC_CACHE_VERS_1);

  sviter = scopedvar_iter_start (scopedvar,
      SV_ITER_HIERARCHY | SV_ITER_SKIP_CURR);
  while ((profname = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    if (mkc_error_chk_err (process->mkcerr)) {
      return;
    }

    process_save_cache_profile (process, fh, tbuff, MKC_SMALL_BUFF_SZ,
        sviter, profname, &tcount);
  }
  scopedvar_iter_finish (sviter);

  sviter = scopedvar_iter_start (scopedvar, SV_ITER_COMPILERS);
  while ((profname = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    if (mkc_error_chk_err (process->mkcerr)) {
      return;
    }

    process_save_cache_profile (process, fh, tbuff, MKC_SMALL_BUFF_SZ,
        sviter, profname, &tcount);
  }
  scopedvar_iter_finish (sviter);

  sviter = scopedvar_iter_start (scopedvar, SV_ITER_NAMESPACE);
  while ((profname = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    if (mkc_error_chk_err (process->mkcerr)) {
      return;
    }

    process_save_cache_profile (process, fh, tbuff, MKC_SMALL_BUFF_SZ,
        sviter, profname, &tcount);
  }
  scopedvar_iter_finish (sviter);

  if (tcount == 0) {
    fprintf (fh, "  ;\n");
  }
  fprintf (fh, "}\n");

  free (cachename);
  free (tbuff);
  fclose (fh);
}

bool
mkc_process_profile_is_current (mkc_process_t *process, value_t *valnm)
{
  char        nm [MKC_VNAME_MAX];
  const char  *profnm;

  if (process->inloadcache) {
    return true;
  }

  scopedvar_value_get_str (process->scopedvar, valnm, nm, sizeof (nm));
  profnm = scopedvar_get_current_profile (process->scopedvar);

  if (strcmp (nm, profnm) == 0 ||
      strcmp (nm, MKC_C_PROF_INTERNAL_NAME) == 0 ||
      strcmp (nm, MKC_C_PROF_DEFAULT_NAME) == 0) {
    return true;
  }

  return false;
}

/* internal routines */

const char *
mkc_process_create_name (mkc_process_t *process, mkc_astnode_token_t asttype,
    char *buff, size_t sz, const char *tag, ...)
{
  char            *p;
  size_t          len;
  size_t          nlen = 0;
  const char      * str;
  va_list         ap;
  mkc_alternate_t * alt;
  mkc_listidx_t   iteridx;
  mkc_listidx_t   aidx;

  va_start (ap, tag);

  /* get the first alternate in the list */
  /* curralt is pointing to the last */
  /* the name of the check comes from the first alternate, */
  /* which has the settings of the base test */
  mkc_list_iter_start (process->attr.alternates, &iteridx);
  aidx = mkc_list_iter_next (process->attr.alternates, &iteridx);
  alt = mkc_list_get_by_idx (process->attr.alternates, aidx);

  /* for chk-package, the name replaces the name of the package */
  if (alt->name != NULL && asttype != MKC_T_CHK_PACKAGE) {
    stpecpy (buff, buff + sz, alt->name);
    va_end (ap);
    return buff;
  }

  p = stpecpy (buff, buff + sz, tag);
  nlen = strlen (buff);

  /* the caller must pass in a NULL terminal indicator */
  while ((str = va_arg (ap, const char *)) != NULL) {
    if (alt->name != NULL && asttype == MKC_T_CHK_PACKAGE) {
      /* special case for chk-package */
      str = alt->name;
    }

    len = strlen (str);

    if (nlen > 0 && *(buff + nlen - 1) != '_') {
      p = stpecpy (p, buff + sz, "_");
      nlen += 1;
    }

    if (*str == '-') {
      ++str;
      len -= 1;
      if (*str == 'W') {
        ++str;
        len -= 1;
      }
    }

    p = stpecpy (p, buff + sz, str);
    nlen += len;
  }
  if (nlen >= 2) {
    if (strcmp (buff + nlen - 2, ".h") == 0 ||
        strcmp (buff + nlen - 2, ".c") == 0 ||
        strcmp (buff + nlen - 2, ".m") == 0 ||
        strcmp (buff + nlen - 2, ".l") == 0 ||
        strcmp (buff + nlen - 2, ".y") == 0) {
      buff [nlen - 2] = '\0';
      nlen -= 2;
    }
  }

  if (nlen >= 4) {
    if (strcmp (buff + nlen - 4, ".cpp") == 0 ||
        strcmp (buff + nlen - 4, ".hpp") == 0) {
      buff [nlen - 4] = '\0';
      nlen -= 4;
    }
  }

  str_clean (buff, nlen);

  return buff;
}

static int
mkc_process_int_checks (mkc_process_t *process)
{
  int                 rc;
  int                 isystype;

  mkc_create_dirs ();

  mkc_log (process->log, MKC_LOG_CHECK, "== internal checks\n");

  /* environment variables : default/comp */

  rc = mkc_chk_compiler_env (process->check);
  if (rc == MKC_OK_CHANGE) {
    return MKC_OK_CHANGE;
  }

  /* check if compiler works */

  rc = mkc_chk_compiler_works (process->check, process->dfltcompiler);
  if (rc != 0) {
    mkc_error_set (process->mkcerr, MKC_ERR_COMPILER_FAILURE, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  mkc_process_attr_clear (process);

  /* compiler id : internal */

  rc = mkc_chk_compiler_id (process->check, process->dfltcompiler);
  if (rc >= 0) {
    process->compid = rc;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "compiler-id", process->compid);

  for (mkc_compiler_id_t i = 0; i < MKC_COMP_ID_MAX; ++i) {
    if (process->compid == i) {
      scopedvar_set_integer (process->scopedvar, SV_T_INTERNAL, compidnames [i], true, MKC_VCTXT_MKC);
      break;
    }
  }

  mkc_process_attr_clear (process);

  /* modern header support : dflt/comp */

  rc = mkc_chk_header_modern (process->check, process->dfltcompiler);
  if (rc != 0) {
    process->headertype = MKC_HEADER_LEGACY;
  }
  process->attr.headertype = process->headertype;
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "header-type", process->headertype);

  mkc_process_attr_clear (process);

  /* system type : internal */

  rc = mkc_chk_system_type (process->check, process->dfltcompiler);
  if (rc >= 0) {
    process->systype = rc;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "system-type", process->systype);

  for (mkc_system_type_t i = 0; i < MKC_SYS_MAX; ++i) {
    if (process->systype == i) {
      scopedvar_set_integer (process->scopedvar, SV_T_INTERNAL, sysnames [i], true, MKC_VCTXT_MKC);
      break;
    }
  }

  mkc_process_attr_clear (process);

  /* object, executable extension : internal */

  if (process->systype == MKC_SYS_WINDOWS) {
    process->objext = ".obj";
    process->exeext = ".exe";
  }
  scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
      MKC_C_OBJEXT, process->objext, MKC_VCTXT_MKC);
  scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
      MKC_C_EXEEXT, process->exeext, MKC_VCTXT_MKC);

  /* shared library extension : internal */

  /* default is .so */
  scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
      MKC_C_SHLIBEXT, ".so", MKC_VCTXT_MKC);
  isystype = process->systype;
  switch (isystype) {
    case MKC_SYS_AIX: {
      scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
          MKC_C_SHLIBEXT, ".a", MKC_VCTXT_MKC);
      break;
    }
    case MKC_SYS_MACOS: {
      scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
          MKC_C_SHLIBEXT, ".dylib", MKC_VCTXT_MKC);
      break;
    }
    case MKC_SYS_WINDOWS: {
      scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
          MKC_C_SHLIBEXT, ".dll", MKC_VCTXT_MKC);
      break;
    }
  }

  /* system id : internal */

  rc = mkc_chk_system_id (process->check, process->dfltcompiler);
  if (rc >= 0) {
    process->sysid = rc;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "system-id", process->sysid);

  for (mkc_system_id_t i = 0; i < MKC_SYS_ID_MAX; ++i) {
    if (process->sysid == i) {
      scopedvar_set_integer (process->scopedvar, SV_T_INTERNAL,
          sysidnames [i], true, MKC_VCTXT_MKC);
      break;
    }
  }

  mkc_process_attr_clear (process);

  /* variadic macro support : dflt/comp */

  rc = mkc_chk_variadic_macro (process->check, process->dfltcompiler);
  if (rc != 0) {
    process->variadicmacro = MKC_NO_VARIADIC_MACRO;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", MKC_C_IVARMACRO, process->variadicmacro);
  scopedvar_set_integer (process->scopedvar, SV_T_SEARCH,
      MKC_C_IVARMACRO, process->variadicmacro, MKC_VCTXT_MKC);

  mkc_process_attr_clear (process);

  /* linux: library location : internal */

  if (process->systype == MKC_SYS_LINUX) {
    rc = mkc_chk_library_location (process->check, process->dfltcompiler);
    if (rc >= 0) {
      process->libloc = rc;
    }
    mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", MKC_C_LIBLOCNAME, process->libloc);
    scopedvar_set_integer (process->scopedvar, SV_T_INTERNAL,
        MKC_C_LIBLOCNAME, process->libloc, MKC_VCTXT_MKC);
  }

  mkc_process_attr_clear (process);

  return MKC_OK;
}

static void
mkc_process_set_defaults (mkc_process_t *process)
{
  /* create internal constants */

  for (mkc_system_type_t i = 0; i < MKC_SYS_MAX; ++i) {
    scopedvar_set_integer (process->scopedvar, SV_T_INTERNAL,
        sysnames [i], false, MKC_VCTXT_MKC);
  }
  for (mkc_system_id_t i = 0; i < MKC_SYS_ID_MAX; ++i) {
    scopedvar_set_integer (process->scopedvar, SV_T_INTERNAL,
        sysidnames [i], false, MKC_VCTXT_MKC);
  }

  scopedvar_set_integer (process->scopedvar, SV_T_INTERNAL,
      MKC_C_LOOPLIMIT, 10000, MKC_VCTXT_MKC);
  scopedvar_set_integer (process->scopedvar, SV_T_INTERNAL,
      MKC_C_LIBLOCNAME, process->libloc, MKC_VCTXT_MKC);

  scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
      "BISON", "bison", MKC_VCTXT_ENV);
  scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
      "CC", "cc", MKC_VCTXT_ENV);
  scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
      "CXX", "c++", MKC_VCTXT_ENV);
  scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
      "FLEX", "flex", MKC_VCTXT_ENV);
  scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
      "OBJC", "cc", MKC_VCTXT_ENV);

  for (mkc_compiler_id_t i = 0; i < MKC_COMP_ID_MAX; ++i) {
    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }
    scopedvar_set_integer (process->scopedvar, SV_T_INTERNAL,
        compidnames [i], false, MKC_VCTXT_MKC);
  }

  scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
      MKC_C_PROFILE_NAME,
      scopedvar_get_current_profile (process->scopedvar),
      MKC_VCTXT_MKC);
// ###      scopedvar_get_current_profile (process->profiles, process->pidx_curr_comp),
}

static void
mkc_process_configure_manual (mkc_process_t *process)
{
  char    *data;
  char    *ndata;
  size_t  fsz = 0;
  FILE    *fh;

  data = fileop_read_file (process->attr.str [MKC_ATTR_INPUT], &fsz, process->mkcerr);
  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_error_set (process->mkcerr, MKC_ERR_FILE_NOT_FOUND,
        errno, process->attr.str [MKC_ATTR_INPUT]);
    return;
  }
  ndata = mkc_process_configure_substitute (process, data);
  free (data);
  fh = fileop_open (process->attr.str [MKC_ATTR_OUTPUT], "w");
  if (fh == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, process->attr.str [MKC_ATTR_OUTPUT]);
    return;
  }

  if (fwrite (ndata, strlen (ndata), 1, fh) != 1) {
    mkc_error_set (process->mkcerr, MKC_ERR_FILE_WRITE_ERROR, errno, NULL);
  }
  fclose (fh);
  free (ndata);
}

static void
mkc_process_configure_auto (mkc_process_t *process, int defzero)
{
  FILE            * fh;
  char            * fname;
  char            * tbuff;
  char            * tp;
  char            projnm [MKC_VNAME_MAX];
  size_t          len;
  scopedvar_t     * scopedvar;
  sv_iter_t       * sviter;
  const char      * profname;

  fname = malloc (MKC_PATH_MAX);
  if (fname == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  tp = process->projectname;
  if (tp == NULL) {
    tp = "project";
  }
  tp = stpecpy (projnm, projnm + sizeof (projnm), tp);
  tp = stpecpy (tp, projnm + sizeof (projnm), "_config");

  if (process->attr.str [MKC_ATTR_OUTPUT] != NULL) {
    stpecpy (fname, fname + MKC_PATH_MAX, process->attr.str [MKC_ATTR_OUTPUT]);
    tp = strrchr (fname, '/');
    if (tp != NULL) {
      tp = stpecpy (projnm, projnm + sizeof (projnm), tp + 1);
      tp = strrchr (projnm, '.');
      if (tp != NULL) {
        *tp = '\0';
      }
    }
  } else {
    snprintf (fname, MKC_PATH_MAX, "%s.h", projnm);
  }

  len = strlen (projnm);
  for (size_t i = 0; i < len; ++i) {
    if (! isalnum ((unsigned char) projnm [i])) {
      projnm [i] = '_';
    } else {
      projnm [i] = toupper (projnm [i]);
    }
  }

  fh = fileop_open (fname, "w");
  if (fh == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, fname);
    return;
  }


  fprintf (fh, "/* built by mkc */\n");
  fprintf (fh, "#ifndef INC_%s_H\n", projnm);
  fprintf (fh, "#define INC_%s_H\n", projnm);
  fprintf (fh, "\n");

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    free (fname);
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  scopedvar = process->scopedvar;

  sviter = scopedvar_iter_start (scopedvar, SV_ITER_HIERARCHY);
  while ((profname = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    mkc_varidx_t    viter;
    mkc_varidx_t    vidx;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    scopedvar_var_iter_start (scopedvar, sviter, &viter);
    while ((vidx = scopedvar_var_iter_next (scopedvar, sviter, &viter)) != MKC_ITER_FINISH) {
      const char  * nm;
      value_t     * value;

      nm = scopedvar_var_iter_get_name (scopedvar, sviter, vidx);
      value = scopedvar_var_iter_get_value (scopedvar, sviter, vidx);
      if (value->vctxt != MKC_VCTXT_CHECK &&
          value->vctxt != MKC_VCTXT_USER_ENABLE) {
        continue;
      }

      if (value->vtype == MKC_VT_INTEGER) {
        int32_t     ival;

        ival = value->ival;
        if (defzero == MKC_AUTO_DEFINE_ZERO || ival != 0) {
          fprintf (fh, "#define %s %" PRId32 "\n", nm, ival);
        }
      } else if (value->vtype == MKC_VT_TIMESTAMP) {
        time_t    tmval;

        tmval = value->tmval;
        if (defzero == MKC_AUTO_DEFINE_ZERO || tmval != 0) {
          fprintf (fh, "#define %s %" PRId64 "\n", nm, tmval);
        }
      } else {
        value_to_str (value, tbuff, MKC_PATH_MAX);
        fprintf (fh, "#define %s \"%s\"\n", nm, tbuff);
      }
    }
  }
  scopedvar_iter_finish (sviter);

  fprintf (fh, "\n");
  fprintf (fh, "#endif /* INC_%s_H */\n", projnm);

  free (fname);
  free (tbuff);
  fclose (fh);
}

static bool
mkc_process_chk_cache (mkc_process_t *process,
    const char *disp, const char *nm)
{
  bool    rc = false;

  /* if the re-test mkc-option is set, then failed tests will be re-tested */
  if (scopedvar_is_defined (process->scopedvar, nm)) {
    if (process->mkcoptions->retest) {
      value_t   *value;

      value = scopedvar_get_value (process->scopedvar, SV_T_SEARCH, nm);
      if (value->vtype == MKC_VT_INTEGER) {
        int32_t   val;

        val = scopedvar_value_get_integer (process->scopedvar, value);
        if (! val) {
          return rc;
        }
      }
    }

    mkc_message ("-- cached: %s : %s\n", disp, nm);
    mkc_log (process->log, MKC_LOG_CHECK, "-- cached: %s : %s\n", disp, nm);
    rc = true;
  }

  return rc;
}

static void
mkc_process_get_path (mkc_process_t *process)
{
  char            *tbuff;
  char            *tpath;
  char            *tokstr;
  const char      *pathdelim = ":";


  if (process->systype == MKC_SYS_WINDOWS) {
    pathdelim = ";";
  }

  tbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  env_get ("PATH", tbuff, MKC_SMALL_BUFF_SZ);

  tpath = str_token (tbuff, pathdelim, &tokstr);
  while (tpath != NULL) {
    fileop_normalize_path (tpath, strlen (tpath));
    scopedvar_append_str_list (process->scopedvar, SV_T_INTERNAL,
        MKC_C_PATH, tpath, MKC_VCTXT_MKC);
    tpath = str_token (NULL, pathdelim, &tokstr);
  }

  free (tbuff);
}

static void
mkc_process_find_executables (mkc_process_t *process)
{
  char            *testpath;
  mkc_prog_chk_t  *chk;
  char            *p;
  mkc_list_t      *pathlist;
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;
  value_t         *valpath;


  testpath = malloc (MKC_PATH_MAX);
  if (testpath == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  valpath = scopedvar_get_value (process->scopedvar, SV_T_INTERNAL, MKC_C_PATH);
  pathlist = valpath->list;

  chk = proglist;
  while (chk->program != NULL) {
    mkc_list_iter_start (pathlist, &iteridx);
    while ((lidx = mkc_list_iter_next (pathlist, &iteridx)) != MKC_ITER_FINISH) {
      value_t   *lvalue;

      lvalue = mkc_list_get_by_idx (pathlist, lidx);
      scopedvar_value_get_str (process->scopedvar, lvalue, testpath, MKC_PATH_MAX);

      p = testpath + strlen (testpath);
      p = stpecpy (p, testpath + MKC_PATH_MAX, "/");
      p = stpecpy (p, testpath + MKC_PATH_MAX, chk->program);
      p = stpecpy (p, testpath + MKC_PATH_MAX, process->exeext);

      if (fileop_exists (testpath)) {
        scopedvar_set_str (process->scopedvar, SV_T_INTERNAL,
            chk->mkcvarname, testpath, MKC_VCTXT_MKC);
        break;
      }
    }
    chk += 1;
  }

  free (testpath);
}

static void
mkc_process_attr_clear (mkc_process_t *process)
{
  mkc_list_free (process->attr.alternates);
  process->attr.alternates = mkc_list_init (MKC_LIST_UNSORTED, mkc_process_alternate_free, NULL, process->mkcerr);
  mkc_process_attr_alternate (process);

  if (mkc_list_size (process->attr.pathlist) > 0) {
    mkc_list_free (process->attr.pathlist);
    process->attr.pathlist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, process->mkcerr);
  }

  if (mkc_list_size (process->attr.replacelist) > 0) {
    mkc_list_free (process->attr.replacelist);
    process->attr.replacelist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, process->mkcerr);
  }

  for (int i = 0; i < MKC_ATTR_MAX; ++i) {
    datafree (process->attr.str [i]);
    process->attr.str [i] = NULL;
  }

  process->attr.define_zero = MKC_AUTO_SKIP_ZERO;
  process->attr.currcompiler = process->dfltcompiler;
  process->attr.localheader = false;
  process->attr.printerrors = false;
  process->attr.negate = false;
}

static mkc_user_regex_t *
mkc_process_user_regex_init (mkc_process_t *process, const char *pattern)
{
  mkc_user_regex_t    *urx;
  mkc_user_regex_t    turx;
  mkc_listidx_t       idx;
  mkc_listidx_t       loc = MKC_LIST_NOTFOUND;

  turx.rx = NULL;
  turx.pattern = (char *) pattern;

  idx = mkc_list_find (process->user_rx_list, &turx, &loc);
  if (idx == MKC_LIST_FOUND) {
    urx = mkc_list_get_by_idx (process->user_rx_list, idx);
    return urx;
  }

  turx.pattern = strdup (pattern);
  if (turx.pattern == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
#if _have_regex
  turx.rx = mkc_regex_init (turx.pattern, MKC_REGEX_NONE, process->mkcerr);
#endif
  urx = mkc_list_set (process->user_rx_list, &turx, sizeof (mkc_user_regex_t), &loc);

  return urx;
}

static void
mkc_process_user_regex_free (void *turx)
{
  mkc_user_regex_t  *urx = turx;

  datafree (urx->pattern);
#if _have_regex
  mkc_regex_free (urx->rx);
#endif
}

static int
mkc_process_user_regex_comp (void *turxa, void *turxb)
{
  mkc_user_regex_t  *urxa = turxa;
  mkc_user_regex_t  *urxb = turxb;

  return strcmp (urxa->pattern, urxb->pattern);
}

static void
mkc_process_dbg_print_var (mkc_process_t *process, const char *profname)
{
  sv_iter_t         * sviter;
  bool              intest = false;
  char              * tbuff;
  scopedvar_t       * scopedvar;
  const char        * svprofname;
  sv_iter_flag_t    itertype;

  tbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  scopedvar = process->scopedvar;

  itertype = SV_ITER_HIERARCHY;
  if (profname != NULL) {
    if (strcmp (profname, "default") == 0) {
      profname = MKC_C_PROF_DEFAULT_NAME;
    } else if (strcmp (profname, "test") == 0) {
      profname = MKC_C_PROF_DEFAULT_NAME;
      intest = true;
      itertype = SV_ITER_HIERARCHY | SV_ITER_SKIP_CURR;
    } else if (strcmp (profname, "testuserprof") == 0) {
      profname = NULL;
      itertype = SV_ITER_USER_PROF;
      intest = true;
    } else if (strcmp (profname, "userprof") == 0) {
      profname = NULL;
      itertype = SV_ITER_USER_PROF;
    } else if (strcmp (profname, "compilers") == 0) {
      profname = NULL;
      itertype = SV_ITER_COMPILERS;
    } else if (strcmp (profname, "ts") == 0) {
      profname = MKC_C_PROF_TIMESTAMP_NAME;
      itertype = SV_ITER_NAMESPACE;
    } else if (strcmp (profname, "dep") == 0) {
      profname = MKC_C_PROF_DEPENDENCY_NAME;
      itertype = SV_ITER_NAMESPACE;
    } else if (strcmp (profname, "paths") == 0) {
      profname = MKC_C_PROF_PATHS_NAME;
      itertype = SV_ITER_NAMESPACE;
    }
  }

  sviter = scopedvar_iter_start (scopedvar, itertype);
  while ((svprofname = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    mkc_varidx_t    viter;
    mkc_varidx_t    vidx;
    bool            hdr = false;

    if (profname != NULL && strcmp (svprofname, profname) != 0) {
      continue;
    }

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    scopedvar_var_iter_start (scopedvar, sviter, &viter);
    while ((vidx = scopedvar_var_iter_next (scopedvar, sviter, &viter)) != MKC_ITER_FINISH) {
      const char  * nm;
      value_t     * value;

      if (! hdr) {
        mkc_compiler_t    compiler;
        const char        *compstr = "";

        compiler = scopedvar_iter_get_compiler (scopedvar, sviter);
        compstr = compiler_get_name (compiler);
        fprintf (stdout, "== %s %s\n", svprofname, compstr);
        hdr = true;
      }

      nm = scopedvar_var_iter_get_name (scopedvar, sviter, vidx);
      value = scopedvar_var_iter_get_value (scopedvar, sviter, vidx);

      if (intest) {
        if (strcmp (nm, "CC") == 0 ||
            strcmp (nm, "CXX") == 0 ||
            strcmp (nm, "OBJC") == 0 ||
            strcmp (nm, "BISON") == 0 ||
            strcmp (nm, "FLEX") == 0 ||
            strcmp (nm, MKC_C_IVARMACRO) == 0) {
          continue;
        }
      }

      value_to_str (value, tbuff, MKC_SMALL_BUFF_SZ);
      if (value->vtype == MKC_VT_INTEGER ||
          value->vtype == MKC_VT_TIMESTAMP ||
          value->vtype == MKC_VT_LIST) {
        fprintf (stdout, "  %s %s\n", nm, tbuff);
      } else {
        fprintf (stdout, "  %s '%s'\n", nm, tbuff);
      }
    }
  }
  scopedvar_iter_finish (sviter);

  free (tbuff);
}

static void
mkc_process_dbg_print_prof (mkc_process_t *process)
{
  scopedvar_t * scopedvar;
  sv_iter_t   * sviter = NULL;
  const char  * profname;

  scopedvar = process->scopedvar;

  fprintf (stdout, "== profiles\n");

  sviter = scopedvar_iter_start (scopedvar, SV_ITER_HIERARCHY | SV_ITER_SKIP_CURR);
  while ((profname = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    sv_type_t  svtype;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    svtype = scopedvar_iter_get_type (scopedvar, sviter);
    fprintf (stdout, "  %s %s\n", scopedvar_type_disp (svtype), profname);
  }
  scopedvar_iter_finish (sviter);

  sviter = scopedvar_iter_start (scopedvar, SV_ITER_USER_PROF);
  while ((profname = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    sv_type_t  svtype;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    svtype = scopedvar_iter_get_type (scopedvar, sviter);
    fprintf (stdout, "  %s %s\n", scopedvar_type_disp (svtype), profname);
  }
  scopedvar_iter_finish (sviter);

  sviter = scopedvar_iter_start (scopedvar, SV_ITER_COMPILERS);
  while ((profname = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    sv_type_t       svtype;
    mkc_compiler_t  compiler;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    svtype = scopedvar_iter_get_type (scopedvar, sviter);
    compiler = scopedvar_iter_get_compiler (scopedvar, sviter);
    fprintf (stdout, "  %s %s %s\n", scopedvar_type_disp (svtype),
        profname, compiler_get_name (compiler));
  }
  scopedvar_iter_finish (sviter);

  sviter = scopedvar_iter_start (scopedvar, SV_ITER_NAMESPACE);
  while ((profname = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    sv_type_t  svtype;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    svtype = scopedvar_iter_get_type (scopedvar, sviter);
    fprintf (stdout, "  %s %s\n", scopedvar_type_disp (svtype), profname);
  }
  scopedvar_iter_finish (sviter);
}

static void
mkc_process_dbg_print_path (mkc_process_t *process)
{
  char    tbuff [MKC_PATH_MAX];

  fprintf (stdout, "== paths\n");
  for (int i = 0; i < MKC_PATH_BUILD_MAX; ++i) {
    path_build (i, tbuff, sizeof (tbuff), NULL, process->mkcerr);
    fprintf (stdout, "  %s %s\n", pathdesc [i], tbuff);
  }
}

static void
mkc_process_dbg_print_int_var (mkc_process_t *process)
{
  fprintf (stdout, "== internal variables\n");
  fprintf (stdout, "  project-name: %s\n", process->projectname);
  fprintf (stdout, "  default-compiler %d/%s\n", process->dfltcompiler, compiler_get_name (process->dfltcompiler));
  fprintf (stdout, "  systype %d\n", process->systype);
  fprintf (stdout, "  sysid %d\n", process->sysid);
  fprintf (stdout, "  compid %d\n", process->compid);
  fprintf (stdout, "  header-type %d\n", process->headertype);
  fprintf (stdout, "  cache-loaded %d\n", process->cacheloaded);
  fprintf (stdout, "  cache-invalidated %d\n", process->cacheinvalidated);
}

static char *
mkc_process_configure_substitute (mkc_process_t *process, char *data)
{
  char          *ndata = NULL;
  mkc_list_t    *rl;

  rl = process->attr.replacelist;
  if (mkc_list_size (rl) == 0) {
    ndata = scopedvar_substitute (process->scopedvar, data, SV_NO_ESCAPE, 0);
  } else {
    mkc_listidx_t   iteridx;
    mkc_listidx_t   lidx;

    ndata = data;

    mkc_list_iter_start (rl, &iteridx);
    while ((lidx = mkc_list_iter_next (rl, &iteridx)) != MKC_ITER_FINISH) {
      value_t   *valstr;
      value_t   *valval;
      char          str [MKC_VNAME_MAX];
      char          val [MKC_VNAME_MAX];
      char          *tdata = NULL;

      valstr = mkc_list_get_by_idx (rl, lidx);
      lidx = mkc_list_iter_next (rl, &iteridx);
      if (lidx == MKC_ITER_FINISH) {
        fprintf (stderr, "ERR: replace-list not paired\n");
        mkc_error_set (process->mkcerr, MKC_ERR_FATAL_ERROR, 0, "replace list not paired");
        return NULL;
      }
      valval = mkc_list_get_by_idx (rl, lidx);
      scopedvar_value_get_str (process->scopedvar, valstr, str, sizeof (str));
      scopedvar_value_get_str (process->scopedvar, valval, val, sizeof (val));
#if _have_regex
      tdata = mkc_regex_replace_literal (ndata, str, val, process->mkcerr);
#endif
      if (ndata != data) {
        datafree (ndata);
      }
      ndata = tdata;
    }
  }
  return ndata;
}

static void
mkc_process_alternate_free (void *tchkcontext)
{
  mkc_alternate_t    *alt = tchkcontext;

  if (alt == NULL) {
    return;
  }

  datafree (alt->name);
  mkc_list_free (alt->hdrlist);
  mkc_list_free (alt->compflags);
  mkc_list_free (alt->linkflags);
}

static void
mkc_process_topo_add_items (mkc_process_t *process,
    toposort_t *topo, mkc_list_t *hlist)
{
  mkc_listidx_t   hiteridx;
  mkc_listidx_t   hidx;

  mkc_list_iter_start (hlist, &hiteridx);
  while ((hidx = mkc_list_iter_next (hlist, &hiteridx)) != MKC_ITER_FINISH) {
    char        **temp;
    const char  *hdr;

    temp = mkc_list_get_by_idx (hlist, hidx);
    hdr = *temp;
    toposort_add_item (topo, hdr);
  }
}

static void
mkc_process_topo_add_deps (mkc_process_t *process,
    toposort_t *topo, char *rbuff, const char *hdr)
{
  mkc_list_t      *elist;
  mkc_list_t      *deplist;
  mkc_listidx_t   diteridx;
  mkc_listidx_t   didx;

  deplist = mkc_list_init (MKC_LIST_UNSORTED, mkc_list_ind_free, NULL, process->mkcerr);

  mkc_check_get_include_deps (process->check,
      process->attr.currcompiler, rbuff, deplist);

  elist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, process->mkcerr);
  scopedvar_set_list (process->scopedvar, SV_T_DEPENDENCY,
      hdr, elist, MKC_VCTXT_MKC);
  mkc_list_free (elist);
  mkc_log (process->log, MKC_LOG_CHECK, "  %s : ", hdr);

  mkc_list_iter_start (deplist, &diteridx);
  while ((didx = mkc_list_iter_next (deplist, &diteridx)) != MKC_ITER_FINISH) {
    char  **temp;
    char  *dep = NULL;

    temp = mkc_list_get_by_idx (deplist, didx);
    dep = *temp;
    scopedvar_append_str_list (process->scopedvar, SV_T_DEPENDENCY,
        hdr, dep, MKC_VCTXT_MKC);
    mkc_log (process->log, MKC_LOG_CHECK, "  %s", dep);
    toposort_add_pair (topo, hdr, dep);
  }
  mkc_log (process->log, MKC_LOG_CHECK, "\n");

  mkc_list_free (deplist);
}

static mkc_list_t *
mkc_process_get_include_list (mkc_process_t *process, mkc_regex_t *rx,
    time_t *ts)
{
  mkc_list_t      *hlist = NULL;
#if _have_regex
  mkc_listidx_t   piteridx;
  mkc_listidx_t   pathidx;
  char            *tbuff;
  char            *path = NULL;
  time_t          newts = 0;

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  path = malloc (MKC_PATH_MAX);
  if (path == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  hlist = mkc_list_init (MKC_LIST_UNSORTED, mkc_list_ind_free, NULL, process->mkcerr);

  mkc_list_iter_start (process->attr.pathlist, &piteridx);
  while ((pathidx = mkc_list_iter_next (process->attr.pathlist, &piteridx)) != MKC_ITER_FINISH) {
    value_t   *valpath = NULL;
    mkc_list_t    *tlist = NULL;
    mkc_listidx_t iteridx;
    mkc_listidx_t idx;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    valpath = mkc_list_get_by_idx (process->attr.pathlist, pathidx);
    scopedvar_value_get_str (process->scopedvar, valpath, path, MKC_PATH_MAX);

    tlist = dir_match (path, rx, process->mkcerr);

    mkc_list_iter_start (tlist, &iteridx);
    while ((idx = mkc_list_iter_next (tlist, &iteridx)) != MKC_ITER_FINISH) {
      char          **temp;
      char          *hdr;
      time_t        tts;
      mkc_listidx_t loc;

      temp = mkc_list_get_by_idx (tlist, idx);
      hdr = *temp;

      snprintf (tbuff, MKC_PATH_MAX, "%s/%s", path, hdr);
      tts = fileop_modtime (tbuff);
      /* convert to milliseconds */
      tts *= 1000;
      if (tts > *ts) {
        hdr = strdup (*temp);
        mkc_list_set (hlist, &hdr, sizeof (char *), &loc);
      }
      if (tts > newts) {
        newts = tts;
      }
    }

    mkc_list_free (tlist);
  }

  /* return the timestamp of the latest include file */
  *ts = newts;

  free (path);
  free (tbuff);
#endif
  return hlist;
}

static const char *
mkc_process_iter_includes (mkc_process_t *process, mkc_list_t *hlist,
    mkc_listidx_t *hiteridx, char *hdr, size_t hsz)
{
  mkc_listidx_t   hidx;
  char            *retval = NULL;

  while ((hidx = mkc_list_iter_next (hlist, hiteridx)) != MKC_ITER_FINISH) {
    char            **temp;
    const char      *thdr;
    mkc_listidx_t   piteridx;
    mkc_listidx_t   pathidx;

    temp = mkc_list_get_by_idx (hlist, hidx);
    thdr = *temp;
    stpecpy (hdr, hdr + hsz, thdr);

// ### check the timestamp...

    mkc_list_iter_start (process->attr.pathlist, &piteridx);
    while ((pathidx = mkc_list_iter_next (process->attr.pathlist, &piteridx)) != MKC_ITER_FINISH) {
      value_t   *valpath;
      const char    *path;
      time_t        fts;

      valpath = mkc_list_get_by_idx (process->attr.pathlist, pathidx);
      path = valpath->sval;

      snprintf (hdr, hsz, "%s/%s", path, thdr);
      if (! fileop_exists (hdr)) {
        continue;
      }

      /* save the path to the header file */
      scopedvar_set_str (process->scopedvar, SV_T_PATHS, thdr, hdr, MKC_VCTXT_MKC);
      fts = fileop_modtime (hdr);
      scopedvar_set_timestamp (process->scopedvar, SV_T_TIMESTAMP, thdr, fts, MKC_VCTXT_MKC);

      retval = hdr + strlen (path) + 1;
      return retval;
    }
  }

  return NULL;
}

static char **
mkc_process_get_flags (mkc_process_t *process, const char *flagname)
{
  mkc_list_t      *tlist;
  char            **flags = NULL;
  int             fsz = 0;
  int             fallocsz = 0;
  char            *lastlibloc;
  char            *str;
  scopedvar_t     *scopedvar;
  sv_iter_t       *sviter = NULL;
  const char      *profnm;

  lastlibloc = malloc (MKC_PATH_MAX);
  if (lastlibloc == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  *lastlibloc = '\0';

  str = malloc (MKC_PATH_MAX);
  if (str == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  *str = '\0';

  tlist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, process->mkcerr);

  scopedvar = process->scopedvar;

  sviter = scopedvar_iter_start (scopedvar, SV_ITER_HIERARCHY);
  while ((profnm = scopedvar_iter_next (scopedvar, sviter)) != NULL) {
    value_t         *value = NULL;
    mkc_listidx_t   fiter;
    mkc_listidx_t   fidx;
    sv_type_t       svtype;


    svtype = scopedvar_iter_get_type (scopedvar, sviter);
    value = scopedvar_get_value (scopedvar, svtype, flagname);
    if (value == NULL || value->vtype != MKC_VT_LIST) {
      continue;
    }

    mkc_list_iter_start (value->list, &fiter);
    while ((fidx = mkc_list_iter_next (value->list, &fiter)) != MKC_ITER_FINISH) {
      value_t   *fval;

      fval = mkc_list_get_by_idx (value->list, fidx);
      scopedvar_value_get_str (scopedvar, fval, str, MKC_PATH_MAX);
      if (! *str) {
        continue;
      }
      if (mkc_process_chk_last_libloc (lastlibloc, MKC_PATH_MAX, str)) {
        /* de-duplication check */
        continue;
      }

      if (fsz >= fallocsz) {
        fallocsz += 10;
        /* always make room for a trailing NULL */
        flags = realloc (flags, sizeof (char *) * (fallocsz + 1));
      }
      flags [fsz + 1] = NULL;
      flags [fsz] = strdup (str);
      fsz += 1;
    }
  }
  scopedvar_iter_finish (sviter);

  mkc_list_free (tlist);
  free (lastlibloc);
  free (str);

  return flags;
}

static bool
mkc_process_chk_last_libloc (char *lastlibloc, size_t sz, const char *str)
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
mkc_process_attr_flags (mkc_process_t *process, value_t *value,
    mkc_list_t *flags, bool inlist)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;

  mkc_list_iter_start (value->list, &iteridx);
  while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    value_t     *lvalue;
    value_t     *tvalue;
    mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    tvalue = scopedvar_value_get_value (process->scopedvar, lvalue);
    if (tvalue->vtype == MKC_VT_LIST) {
      mkc_process_attr_flags (process, tvalue, flags, true);
      if (! inlist) {
        scopedvar_temp_value_free (tvalue);
      }
      return;
    } else if (tvalue->vtype == MKC_VT_STRING ||
        tvalue->vtype == MKC_VT_STATIC_STRING ||
        tvalue->vtype == MKC_VT_QUOTED_STRING) {
      if (! *tvalue->sval) {
        continue;
      }
    } else {
      mkc_error_set (process->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
    }
    mkc_list_set (flags, tvalue, sizeof (value_t), &loc);
    if (! inlist) {
      scopedvar_temp_value_free (tvalue);
    }
  }
}

static void
mkc_process_source_file (mkc_process_t *process, const char *target,
    const char *srcfn)
{
  time_t          ts;
  char            *rbuff;
  mkc_list_t      *deplist;
  size_t          fsz = 0;
  mkc_compiler_t  tcompiler = process->dfltcompiler;
  mkc_listidx_t   diteridx;
  mkc_listidx_t   didx;
  char            objname [MKC_VNAME_MAX];
  char            *p;
  char            *tp;


// ### process source files
//    - includes
//    - create object path
//    - timestamps
//    - dependencies

  deplist = mkc_list_init (MKC_LIST_UNSORTED, mkc_list_ind_free, NULL, process->mkcerr);

  p = stpecpy (objname, objname + sizeof (objname), srcfn);
  tp = strrchr (objname, '.');
  if (tp != NULL) {
    *tp = '\0';
    stpecpy (p, objname + sizeof (objname), process->objext);
  }
  ts = fileop_modtime (srcfn);

//  mkc_pvar_profile_select_idx (process->pvar, process->pidx_ts);
  scopedvar_set_timestamp (process->scopedvar, SV_T_TIMESTAMP,
      srcfn, ts, MKC_VCTXT_MKC);
  rbuff = fileop_read_file (srcfn, &fsz, process->mkcerr);
  mkc_check_get_include_deps (process->check, tcompiler, rbuff, deplist);

//  scopedvar_append_str_list (process->scopedvar, SV_T_DEPENDENCY,
//      target, srcfn, MKC_VCTXT_MKC);

  mkc_list_iter_start (deplist, &diteridx);
  while ((didx = mkc_list_iter_next (deplist, &diteridx)) != MKC_ITER_FINISH) {
    char  **temp;
    char  *dep;

    temp = mkc_list_get_by_idx (deplist, didx);
    dep = *temp;
// ### need full path to include file...
//    ts = fileop_modtime (dep);
//    mkc_pvar_profile_select_idx (process->pvar, process->pidx_ts);
//    scopedvar_set_timestamp (process->pvar, srcfn, ts, MKC_VCTXT_MKC);
//    mkc_pvar_profile_select_idx (process->pvar, process->pidx_deps);
//    tlist = scopedvar_append_str_list (process->scopedvar, SV_T_DEPENDENCY,
//        target, dep, MKC_VCTXT_MKC);
  }

  mkc_list_free (deplist);
  free (rbuff);
}

static void
process_save_cache_profile (mkc_process_t *process, FILE *fh,
    char *tbuff, size_t sz,
    sv_iter_t *sviter, const char *profname, int *tcount)
{
  scopedvar_t   *scopedvar;
  mkc_varidx_t  viter;
  mkc_varidx_t  vidx;
  int           count = 0;
  sv_type_t     svtype;

  scopedvar = process->scopedvar;

  fprintf (fh, "  profile %s {\n", profname);
  svtype = scopedvar_iter_get_type (scopedvar, sviter);
  if (svtype == SV_T_CURR_PROF_COMPILER) {
    mkc_compiler_t    compiler;

    compiler = scopedvar_iter_get_compiler (scopedvar, sviter);
    if (compiler != MKC_COMPILER_GENERAL) {
      fprintf (fh, "    compiler %s;\n", compiler_get_name (compiler));
    }
  }

  scopedvar_var_iter_start (scopedvar, sviter, &viter);
  while ((vidx = scopedvar_var_iter_next (scopedvar, sviter, &viter)) != MKC_ITER_FINISH) {
    const char    *nm;
    value_t       *value;
    const char    *vctxtstr = "";

    nm = scopedvar_var_iter_get_name (scopedvar, sviter, vidx);
    value = scopedvar_var_iter_get_value (scopedvar, sviter, vidx);
    value_to_str (value, tbuff, sz);

    if (value->vtype == MKC_VT_INTEGER ||
        value->vtype == MKC_VT_TIMESTAMP ||
        value->vtype == MKC_VT_LIST) {
      fprintf (fh, "    set '%s' %s ", nm, tbuff);
    } else {
      fprintf (fh, "    set '%s' '%s' ", nm, tbuff);
    }
    vctxtstr = value_ctxt_str (value->vctxt);
    fprintf (fh, "{\n      context %s;", vctxtstr);
    if (svtype > SV_T_NAMESPACE) {
      fprintf (fh, " namespace %s;", scopedvar_type_disp (svtype));
    }
    fprintf (fh, "\n    }\n");
    ++count;
    *tcount += 1;
  }

  if (count == 0) {
    fprintf (fh, "    ;\n");
  }
  fprintf (fh, "  }\n\n");
  fprintf (fh, "\n");
}
