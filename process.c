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

#include "alternate.h"
#include "asttoken.h"
#include "attribute.h"
#include "chararr.h"
#include "compile.h"
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
#include "process.h"
#include "mkc_regex.h"
#include "strutil.h"
#include "mkc_util.h"
#include "mkc_var.h"      // for debugging
#include "pathutil.h"
#include "scopedvar.h"
#include "target.h"
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
  list_t      * namelist;
  value_t     * listval;      // list or range
  value_t     tvalue;
  listidx_t   iteridx;
} mkc_foreach_t;

typedef struct process_t {
  scopedvar_t       * sv;
  compile_t         * compile;
  mkc_check_t       * check;
  mkc_context_t     * context;
  target_t          * target;
  mkc_error_t       * mkcerr;
  mkc_log_t         * log;
  mkc_option_t      * mkcoptions;
  char              * projectname;
  const char        * objext;
  const char        * exeext;
  mkc_regex_t       * rxshellvar;
  mkc_regex_t       * rxincguard;
  list_t            * user_rx_list;
  mkc_attribute_t   attr;
  /* internal */
  int64_t           mkc_ts;
  mkc_compiler_t    dfltcompiler;
  mkc_system_type_t systype;
  mkc_system_id_t   sysid;
  mkc_compiler_id_t compid;
  mkc_lib_loc_t     libloc;
  mkc_header_t      headertype;
  bool              cacheinvalidated;
  bool              cacheloaded;
  bool              cleaned;
  bool              compiler_mm;
  bool              inloadcache;
  bool              mkc_changed;
  bool              mkc_ts_checked;
  bool              reset_stage;
  bool              variadicmacro;
} process_t;

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
static char const * const MKC_C_CHK_INC_DEPS_TS = "MKC_I_CHK_INC_DEPS_TS";
static char const * const MKC_C_CHK_INC_COMPILE_TS = "MKC_I_CHK_INC_COMPILE_TS";
static char const * const MKC_C_CHK_INC_GUARDS_TS = "MKC_I_CHK_INC_GUARDS_TS";

static void process_save_cache_profile (process_t *process, FILE *fh, sv_iter_t *sviter, const char *profname, int *tcount);

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

static void process_attr_clear (process_t *process);
static void process_user_regex_free (void *turx);
static int process_user_regex_comp (void *turxa, void *turxb);
const char * process_create_name (process_t *process, astnode_token_t asttype, char *buff, size_t sz, const char *tag, ...);
static int process_initial_checks (process_t *process);
static void process_set_defaults (process_t *process);
static void process_configure_manual (process_t *process);
static void process_configure_auto (process_t *process, int defzero);
static bool process_chk_cache (process_t *process, const char *disp, const char *nm);
static void process_get_path (process_t *process);
static void process_find_executables (process_t *process);

static mkc_user_regex_t *process_user_regex_init (process_t *process, const char *pattern);
static void process_user_regex_free (void *turx);
static int process_user_regex_comp (void *turxa, void *turxb);

static char * process_configure_substitute (process_t *process, char *data);
static void process_alternate_free (void *talt);
static void process_list_to_flags (process_t *process, value_t *value, list_t *flags, bool inlist);
static void process_check_mkc_timestamp (process_t *process);
static void process_clean_check (process_t *process);

static void process_dbg_print_var (process_t *process, const char *pname);
static void process_dbg_print_prof (process_t *process, sv_iter_flag_t sviterflag);
static void process_dbg_print_path (process_t *process);
static void process_dbg_print_int_var (process_t *process);
static void process_dbg_print_info (process_t *process);


MKC_NODISCARD
process_t *
process_init (scopedvar_t *sv,
    mkc_log_t *log, mkc_context_t *context,
    mkc_option_t *mkcoptions, mkc_error_t *mkcerr)
{
  process_t     *process;
  int               rc;
  mstime_t          starttm;
  char              tbuff [MKC_PATH_MAX];

  mstimestart (&starttm);
  process = malloc (sizeof (process_t));

  process->sv = sv;
  /* at this point, the default compiler is not known */
  process->dfltcompiler = MKC_COMPILER_C;
  process->log = log;
  process->context = context;
  process->mkcoptions = mkcoptions;
  process->check = NULL;
  process->target = NULL;
  process->objext = ".o";
  process->exeext = "";
  process->projectname = NULL;
  process->rxshellvar = NULL;
  process->rxincguard = NULL;
  process->user_rx_list = list_init (MKC_LIST_SORTED,
      process_user_regex_free, process_user_regex_comp,
      sizeof (mkc_user_regex_t), mkcerr);

  process->attr.compid = process->compid;
  process->attr.currcompiler = process->dfltcompiler;
  process->attr.headertype = process->headertype;
  process->attr.alternates = list_init (MKC_LIST_UNSORTED,
      process_alternate_free, NULL, sizeof (mkc_alternate_t), mkcerr);
  process_attr_alternate (process);
  process->attr.pathlist = list_init (MKC_LIST_UNSORTED, NULL, NULL,
      sizeof (value_t), mkcerr);
  process->attr.replacelist = list_init (MKC_LIST_UNSORTED, NULL, NULL,
      sizeof (value_t), mkcerr);
  process->attr.sourcelist = list_init (MKC_LIST_UNSORTED, NULL, NULL,
      sizeof (value_t), mkcerr);
  for (int i = 0; i < MKC_ATTR_MAX; ++i) {
    process->attr.str [i] = NULL;
  }
  process->attr.define_zero = MKC_AUTO_SKIP_ZERO;
  process->attr.display = false;
  process->attr.localheader = false;
  process->attr.negate = false;
  process->attr.printerrors = false;

  process->mkc_ts = 0;
  process->cleaned = false;
  process->cacheloaded = false;
  process->cacheinvalidated = false;
  process->inloadcache = false;
  process->mkc_changed = false;
  process->mkc_ts_checked = false;
  process->reset_stage = false;
  process->mkcerr = mkcerr;

  process->systype = MKC_SYS_UNKNOWN;
  process->sysid = MKC_SYS_ID_NOTSET;
  process->compid = MKC_COMP_ID_UNKNOWN;
  process->libloc = MKC_LIB_LOC_NOTSET;
  process->headertype = MKC_HEADER_MODERN;
  process->attr.headertype = process->headertype;
  process->variadicmacro = MKC_VARIADIC_MACRO_SUPPORTED;
  process->compiler_mm = false;

  path_build (MKC_PATH_EXEC_PATH, tbuff, sizeof (tbuff), mkcoptions->mkc_filename, mkcerr);
  process->mkc_ts = fileop_modtime (tbuff);

  process->compile = compile_init (process->sv,
      &process->attr, log, mkcoptions, mkcerr);
  if (process->compile == NULL) {
    process_free (process);
    return NULL;
  }

  process->check = mkc_check_init (process->sv,
      process->compile,
      &process->attr, log, mkcerr);
  if (process->check == NULL) {
    process_free (process);
    return NULL;
  }

  process->target = target_init (process->sv,
      process->compile,
      &process->attr, log, mkcerr);
  if (process->target == NULL) {
    process_free (process);
    return NULL;
  }

  process_set_defaults (process);
  rc = process_initial_checks (process);
  if (rc < 0) {
    process_free (process);
    return NULL;
  }

  process_get_path (process);
  process_find_executables (process);
  mkc_chk_getconf (process->check);

  {
    char    tbuff [40];
    int64_t  etm;

    etm = mstimeend (&starttm);
    mkc_elapsed_disp (etm, tbuff, sizeof (tbuff));
    mkc_message (MKC_V_BASIC, "-- mkc internal setup: %s\n", tbuff);
    mkc_log (process->log, MKC_LOG_STATISTICS,
        "-- mkc internal setup: %s\n", tbuff);
  }

  mkc_log (process->log, MKC_LOG_CHECK, "== end internal checks\n");
  path_build (MKC_PATH_MKCFILES, tbuff, sizeof (tbuff),
      "mkc-log.txt", mkcerr);
  mkc_log_open (log, tbuff, process->mkcoptions->loglevel);

  return process;
}

void
process_free (process_t *process)
{
  if (process == NULL) {
    return;
  }

  if (process->check != NULL) {
    mkc_check_free (process->check);
  }
  if (process->compile != NULL) {
    compile_free (process->compile);
  }
  if (process->target != NULL) {
    target_free (process->target);
  }
  datafree (process->projectname);

  process_attr_clear (process);
  list_free (process->attr.alternates);
  list_free (process->attr.pathlist);
  list_free (process->attr.replacelist);
  list_free (process->attr.sourcelist);

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
  list_free (process->user_rx_list);
  free (process);
}

int32_t
process_condition (process_t *process, value_t *value)
{
  int32_t   rval;

  if (process == NULL) {
    return 0;
  }

  rval = sv_value_get_integer (process->sv, value);
  return rval;
}

void
process_range_init (process_t *process,
    value_t *value, value_t *beg, value_t *end, value_t *incr)
{
  int32_t     ibeg, iend, iincr;

  ibeg = sv_value_get_integer (process->sv, beg);
  iend = sv_value_get_integer (process->sv, end);
  iincr = sv_value_get_integer (process->sv, incr);
  if (mkc_error_chk_err (process->mkcerr)) {
    return;
  }
  value_range_init (value, ibeg, iend, iincr);
}

int32_t
process_num_op (process_t *process, astnode_token_t asttype,
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
      value_to_str (vala, tbuff, sizeof (tbuff), 0));
  mkc_log (process->log, MKC_LOG_PROCESS, "  p-num-op-b: %s\n",
      value_to_str (valb, tbuff, sizeof (tbuff), 0));
  ivala = sv_value_get_integer (process->sv, vala);
  ivalb = sv_value_get_integer (process->sv, valb);
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
process_str_op (process_t *process, astnode_token_t asttype,
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
      value_to_str (vala, stra, sizeof (stra), 0));
  mkc_log (process->log, MKC_LOG_PROCESS, "  p-str-op-b: %s\n",
      value_to_str (valb, strb, sizeof (strb), 0));
  sv_value_get_str (process->sv, vala, stra, sizeof (stra));
  sv_value_get_str (process->sv, valb, strb, sizeof (strb));
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

      urx = process_user_regex_init (process, strb);
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
process_unary_op (process_t *process, astnode_token_t asttype,
    value_t *vala)
{
  int32_t     result = 0;
  int32_t     ivala = 0;
  int         iasttype = asttype;

  if (process == NULL) {
    return 0;
  }

  ivala = sv_value_get_integer (process->sv, vala);
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
process_other_op (process_t *process, astnode_token_t asttype,
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
  *tbuff = '\0';

  switch (iasttype) {
    case MKC_T_OP_FILE_EXISTS: {
      sv_value_get_str (process->sv, vala, tbuff, MKC_PATH_MAX);
      result = fileop_exists (tbuff);
      break;
    }
    case MKC_T_OP_IS_DEFINED: {
      sv_value_get_str (process->sv, vala, tbuff, MKC_PATH_MAX);
      result = sv_is_defined (process->sv, SV_T_SEARCH, tbuff, NULL);
      break;
    }
    case MKC_T_OP_IS_DIRECTORY: {
      sv_value_get_str (process->sv, vala, tbuff, MKC_PATH_MAX);
      result = fileop_is_directory (tbuff);
      break;
    }
    case MKC_T_OP_IS_DICT: {
      sv_value_get_str (process->sv, vala, tbuff, MKC_PATH_MAX);
      result = sv_var_is_dict (process->sv, tbuff);
      break;
    }
    case MKC_T_OP_IS_LIST: {
      sv_value_get_str (process->sv, vala, tbuff, MKC_PATH_MAX);
      result = sv_var_is_list (process->sv, tbuff);
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
process_include (process_t *process,
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
  *fname = '\0';

  sv_value_get_str (process->sv, valfn, fname, MKC_PATH_MAX);
  if (mkc_error_chk_err (process->mkcerr)) {
    free (fname);
    return;
  }
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
  *tbuff = '\0';

  if (valpath != NULL) {
    sv_value_get_str (process->sv, valpath, tbuff, MKC_PATH_MAX);
    if (mkc_error_chk_err (process->mkcerr)) {
      return;
    }
    p = stpecpy (p, buff + sz, tbuff);
    p = stpecpy (p, buff + sz, "/");
    p = stpecpy (p, buff + sz, fname);
    if (! fileop_exists (buff)) {
      mkc_error_set (process->mkcerr, MKC_ERR_FILE_NOT_FOUND, 0, NULL);
    }
  }

  if (valpath == NULL) {
    path_build (MKC_PATH_MKC_SHR_UNITS, tbuff, MKC_PATH_MAX, fname, process->mkcerr);
    if (fileop_exists (tbuff)) {
      p = stpecpy (buff, buff + sz, tbuff);
    }
  }

  free (tbuff);
  free (fname);
}

/* control statements */

mkc_foreach_t *
process_stmt_foreach_setup (process_t *process,
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

  sv_push (process->sv, SV_T_LOCAL, "local-foreach");
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

    value = sv_value_get_list_value (process->sv, valnm);
    pforeach->namelist = value->list;
  }
  if (vallist != NULL) {
    value_t   *value;

    value = vallist;
    pforeach->listval = sv_value_get_list_value (process->sv, vallist);
    value_iter_start (value, &pforeach->iteridx);
  }

  return pforeach;
}

bool
process_stmt_foreach (process_t *process, mkc_foreach_t *pforeach)
{
  listidx_t   niteridx;
  listidx_t   nidx;
  bool            cont = true;

  list_iter_start (pforeach->namelist, &niteridx);
  while ((nidx = list_iter_next (pforeach->namelist, &niteridx)) != MKC_ITER_FINISH) {
    value_t     *nval = NULL;
    listidx_t   rc;

    nval = list_get_by_idx (pforeach->namelist, nidx);

    rc = value_iter_next (pforeach->listval, &pforeach->tvalue, &pforeach->iteridx);
    if (rc == MKC_ITER_FINISH) {
      cont = false;
      break;
    }
    process_local_set (process, nval, &pforeach->tvalue);
  }

  return cont;
}

void
process_stmt_foreach_finish (process_t *process, mkc_foreach_t *pforeach)
{
  sv_pop (process->sv);
  free (pforeach);
}

/* statements */

int
process_stmt_chk_inc_compile (process_t *process)
{
  int               rc = MKC_ERR_FAILURE;
#if _have_regex
  list_t        * hlist = NULL;
  listidx_t     hiteridx;
  char              * hdrpath;
  const char        * hdr;
  chararr_t         * cflags = NULL;
  chararr_t         * include_paths = NULL;
  int64_t           ts = 0;
  int64_t           chkinccompts = 0;
  int               count = 0;
  mkc_user_regex_t  * urx;

  process_check_mkc_timestamp (process);
  process_clean_check (process);

  if (process->attr.str [MKC_ATTR_MATCH] == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "match");
    process_attr_clear (process);
    return rc;
  }

  hdrpath = malloc (MKC_PATH_MAX);
  if (hdrpath == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    process_attr_clear (process);
    return rc;
  }
  *hdrpath = '\0';

  urx = process_user_regex_init (process, process->attr.str [MKC_ATTR_MATCH]);
  if (mkc_error_chk_err (process->mkcerr)) {
    process_attr_clear (process);
    free (hdrpath);
    return rc;
  }

  process->attr.localheader = true;
  process->attr.printerrors = true;

  include_paths = chararr_init (process->mkcerr);
  chararr_set_freeinternals (include_paths);
  cflags = target_get_flags (process->target, MKC_C_CFLAGS, include_paths);

  hlist = target_get_include_list (process->target, include_paths, urx->rx, &ts);

  /* the chk-inc-compile timestamp is also needed, */
  /* otherwise, if never run, the compile check will skip include */
  /* files that are not out of date */
  /* this will happen if chk-inc-compile is not the first chk-inc */
  if (sv_is_defined (process->sv, SV_T_INTERNAL,
      MKC_C_CHK_INC_COMPILE_TS, NULL)) {
    chkinccompts = sv_get_timestamp (process->sv, SV_T_INTERNAL,
          MKC_C_CHK_INC_COMPILE_TS);
  }

  list_iter_start (hlist, &hiteridx);
  while ((hdr = target_iter_includes (process->target, hlist,
      &hiteridx, hdrpath, MKC_PATH_MAX)) != NULL) {
    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    if (chkinccompts > 0 && target_check_dependency_timestamp (
        process->target, hdr, hdrpath) == TARGET_CURRENT) {
      continue;
    }

    count += 1;
    rc = mkc_chk_header (process->check, process->attr.currcompiler, hdr,
        cflags);
    if (rc != MKC_OK) {
      mkc_error_set (process->mkcerr, MKC_ERR_INCLUDE_COMPILE_FAIL, 0, hdrpath);
      break;
    }
  }

  if (count == 0) {
    mkc_message (MKC_V_BASIC, "-- cached: check_include_compile\n");
    mkc_log (process->log, MKC_LOG_CHECK, "-- cached: check_include_compile\n");
  } else {
    ts = mstime ();
    sv_set_timestamp (process->sv, SV_T_INTERNAL,
        MKC_C_CHK_INC_COMPILE_TS, NULL, ts, MKC_VCTXT_MKC);

    mkc_message (MKC_V_BASIC, "-- check_include_compile - %s (%d)\n",
        mkc_success_msg (rc), count);
    mkc_log (process->log, MKC_LOG_CHECK, "-- check_include_compile - %s (%d)\n",
        mkc_success_msg (rc), count);
  }

  chararr_free (cflags);
  chararr_free (include_paths);
  free (hdrpath);
#endif
  process_attr_clear (process);
  return rc;
}

int
process_stmt_chk_inc_deps (process_t *process)
{
  list_t            * hlist = NULL;
  toposort_t        * topo = NULL;
  listidx_t         hiteridx;
  int               rc = MKC_ERR_FAILURE;
  char              * hdrpath = NULL;
  const char        * hdr;
  int64_t           ts;
  mkc_user_regex_t  * urx;
  chararr_t         * cflags;
  chararr_t         * include_paths;

  process_check_mkc_timestamp (process);
  process_clean_check (process);

  if (process->attr.str [MKC_ATTR_MATCH] == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "match");
    process_attr_clear (process);
    return rc;
  }

  mkc_log (process->log, MKC_LOG_CHECK, "== chk-include-deps\n");

  urx = process_user_regex_init (process, process->attr.str [MKC_ATTR_MATCH]);
  if (mkc_error_chk_err (process->mkcerr)) {
    process_attr_clear (process);
    return rc;
  }

  include_paths = chararr_init (process->mkcerr);
  chararr_set_freeinternals (include_paths);
  cflags = target_get_flags (process->target, MKC_C_CFLAGS, include_paths);

  mkc_message (MKC_V_INFO, "-- check_include_dependencies: getting dependencies\n");

  /* the returned timestamp will be used to determine */
  /* if a check needs to be made */
  /* target_get_include_list will update the saved timestamps */
  ts = 0;
  hlist = target_get_include_list (process->target, include_paths, urx->rx, &ts);
  /* ts now holds the timestamp of the latest modification time in ms */

  if (sv_is_defined (process->sv, SV_T_INTERNAL,
      MKC_C_CHK_INC_DEPS_TS, NULL)) {
    int64_t    cachedts;

    cachedts = sv_get_timestamp (process->sv, SV_T_INTERNAL,
        MKC_C_CHK_INC_DEPS_TS);

    if (cachedts > ts) {
      chararr_free (cflags);
      chararr_free (include_paths);
      mkc_message (MKC_V_BASIC, "-- cached: check_include_dependencies\n");
      mkc_log (process->log, MKC_LOG_CHECK, "-- cached: check_include_dependencies\n");
      process_attr_clear (process);
      return rc;
    }
  }

  hdrpath = malloc (MKC_PATH_MAX);
  if (hdrpath == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    chararr_free (cflags);
    chararr_free (include_paths);
    process_attr_clear (process);
    return rc;
  }
  *hdrpath = '\0';

  topo = toposort_init (process->mkcerr);
  target_topo_add_items (process->target, topo, hlist);

  list_iter_start (hlist, &hiteridx);
  while ((hdr = target_iter_includes (process->target, hlist, &hiteridx,
      hdrpath, MKC_PATH_MAX)) != NULL) {
    if (mkc_error_chk_err (process->mkcerr)) {
      chararr_free (cflags);
      chararr_free (include_paths);
      process_attr_clear (process);
      toposort_free (topo);
      free (hdrpath);
      return rc;
    }

    if (target_check_dependency_timestamp (
        process->target, hdr, hdrpath) == TARGET_OUT_OF_DATE) {
      target_flag_t   tgtflags = TARGET_IGNORE_SYS_INC;

      if (process->compiler_mm) {
        tgtflags |= TARGET_USE_MM;
      }

      target_get_dependencies (process->target,
          process->attr.currcompiler, hdrpath, hdrpath, tgtflags, cflags);
    }

    target_topo_add_deps (process->target, topo, hdrpath);
  }

  rc = toposort (topo);

  if (rc == MKC_OK) {
    ts = mstime ();
    sv_set_timestamp (process->sv, SV_T_INTERNAL,
        MKC_C_CHK_INC_DEPS_TS, NULL, ts, MKC_VCTXT_MKC);
  }

  mkc_message (MKC_V_BASIC, "-- check_include_dependencies - %s\n", mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check_include_dependencies - %s\n",
      mkc_success_msg (rc));

  chararr_free (cflags);
  chararr_free (include_paths);
  toposort_free (topo);
  free (hdrpath);
  process_attr_clear (process);
  return rc;
}

int
process_stmt_chk_inc_guards (process_t *process)
{
  int               rc = MKC_ERR_FAILURE;
#if _have_regex
  list_t            * hlist = NULL;
  listidx_t         hiteridx;
  char              * rbuff;
  char              * hdrpath;
  const char        * hdr;
  char              ** match = NULL;
  int               matchcount;
  list_t            * guardlist = NULL;
  int64_t           ts;
  int               count = 0;
  mkc_user_regex_t  * urx;
  chararr_t         * cflags;
  chararr_t         * include_paths;

  process_check_mkc_timestamp (process);
  process_clean_check (process);

  if (process->attr.str [MKC_ATTR_MATCH] == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "match");
    process_attr_clear (process);
    return rc;
  }

  guardlist = list_init (MKC_LIST_SORTED, list_ind_free,
      list_ind_compare, sizeof (char *), process->mkcerr);

  if (process->rxincguard == NULL) {
    process->rxincguard = mkc_regex_init (
        "^# *ifndef +([[:alnum:]_][[:alnum:]_]*)[\r\n]+# *define +\\g1[\r]*$",
        MKC_REGEX_MULTILINE, process->mkcerr);
  }

  if (mkc_error_chk_err (process->mkcerr)) {
    process_attr_clear (process);
    list_free (guardlist);
    return rc;
  }

  urx = process_user_regex_init (process, process->attr.str [MKC_ATTR_MATCH]);
  if (mkc_error_chk_err (process->mkcerr)) {
    list_free (guardlist);
    process_attr_clear (process);
    return rc;
  }

  include_paths = chararr_init (process->mkcerr);
  chararr_set_freeinternals (include_paths);
  cflags = target_get_flags (process->target, MKC_C_CFLAGS, include_paths);

  rc = MKC_OK;
  ts = 0;
  /* as chk-inc-guards compares all guards to check for duplicates */
  /* the returned timestamp will be used to determine */
  /* if a check needs to be made */
  hlist = target_get_include_list (process->target, include_paths, urx->rx, &ts);

  if (sv_is_defined (process->sv, SV_T_INTERNAL,
      MKC_C_CHK_INC_GUARDS_TS, NULL)) {
    int64_t    cachedts;

    cachedts = sv_get_timestamp (process->sv, SV_T_INTERNAL,
        MKC_C_CHK_INC_GUARDS_TS);

    if (cachedts > ts) {
      mkc_message (MKC_V_BASIC, "-- cached: check_include_guards\n");
      mkc_log (process->log, MKC_LOG_CHECK, "-- cached: check_include_guards\n");

      list_free (guardlist);
      process_attr_clear (process);
      chararr_free (cflags);
      chararr_free (include_paths);
      return rc;
    }
  }

  hdrpath = malloc (MKC_PATH_MAX);
  if (hdrpath == NULL) {
    list_free (guardlist);
    chararr_free (cflags);
    chararr_free (include_paths);
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    process_attr_clear (process);
    return rc;
  }
  *hdrpath = '\0';

  list_iter_start (hlist, &hiteridx);
  while ((hdr = target_iter_includes (process->target, hlist, &hiteridx,
      hdrpath, MKC_PATH_MAX)) != NULL) {
    char            *tp;
    listidx_t   idx;
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
      idx = list_find (guardlist, &tp);
      if (idx != MKC_LIST_NOTFOUND) {
        mkc_error_set (process->mkcerr, MKC_ERR_INCLUDE_GUARD_DUPLICATE, 0, hdrpath);
        free (tp);
        rc = MKC_ERR_FAILURE;
      } else {
        list_set (guardlist, &tp);
      }
    }
    mkc_regex_get_free (match);
    free (rbuff);
  }

  ts = mstime ();
  sv_set_timestamp (process->sv, SV_T_INTERNAL,
      MKC_C_CHK_INC_GUARDS_TS, NULL, ts, MKC_VCTXT_MKC);

  mkc_message (MKC_V_BASIC, "-- check_include_guards - %s (%d)\n",
      mkc_success_msg (rc), count);
  mkc_log (process->log, MKC_LOG_CHECK, "-- check_include_guards - %s (%d)\n",
      mkc_success_msg (rc), count);

  chararr_free (cflags);
  chararr_free (include_paths);
  list_free (guardlist);
  free (hdrpath);
#endif
  process_attr_clear (process);
  return rc;
}

void
process_stmt_build (process_t *process, value_t *vallist)
{
  list_t      * blist;
  char            * tbuff;

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  mkc_create_mkcfiles (tbuff, MKC_PATH_MAX, process->mkcerr);
  free (tbuff);

  blist = list_init (MKC_LIST_UNSORTED, NULL, NULL, sizeof (value_t), process->mkcerr);
  process_list_to_flags (process, vallist, blist, false);
  process->attr.display = true;
  process->attr.printerrors = true;

  target_build (process->target, blist);

  list_free (blist);
  process_attr_clear (process);
  return;
}

void
process_stmt_configure (process_t *process)
{
  int       defzero = MKC_AUTO_SKIP_ZERO;


  if (process->attr.str [MKC_ATTR_METHOD] == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "method");
    process_attr_clear (process);
    return;
  }

  if (strcmp (process->attr.str [MKC_ATTR_METHOD], "manual") == 0) {
    if (process->attr.str [MKC_ATTR_INPUT] == NULL) {
      mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "input");
      process_attr_clear (process);
      return;
    }
    if (process->attr.str [MKC_ATTR_OUTPUT] == NULL) {
      mkc_error_set (process->mkcerr, MKC_ERR_MISSING_ATTRIBUTE, 0, "output");
      process_attr_clear (process);
      return;
    }
  }

  if (process->cacheloaded &&
      ! process->cacheinvalidated &&
      ! process->mkc_changed) {
    mkc_message (MKC_V_BASIC, "-- configure: not required\n");
    return;
  }

  defzero = process->attr.define_zero;

  if (strcmp (process->attr.str [MKC_ATTR_METHOD], "auto") == 0) {
    process_configure_auto (process, defzero);
  } else if (strcmp (process->attr.str [MKC_ATTR_METHOD], "manual") == 0) {
    process_configure_manual (process);
  } else {
    mkc_error_set (process->mkcerr, MKC_ERR_PROC_INVALID_METHOD, 0, NULL);
  }

  process_attr_clear (process);
  return;
}

int
process_stmt_debug (process_t *process,
    value_t *value, value_t *subvalue)
{
  char    tbuff [MKC_VNAME_MAX];

  sv_value_get_str (process->sv, value, tbuff, sizeof (tbuff));
  if (mkc_error_chk_err (process->mkcerr)) {
    return false;
  }

  if (strcmp (tbuff, "null") == 0) {
    /* do nothing */ ;
  }
  if (strcmp (tbuff, "printprof") == 0) {
    process_dbg_print_prof (process, SV_ITER_PROFILES);
  }
  if (strcmp (tbuff, "printhierarchy") == 0) {
    process_dbg_print_prof (process, SV_ITER_HIERARCHY);
  }
  if (strcmp (tbuff, "printvar") == 0) {
    sv_value_get_str (process->sv, subvalue, tbuff, sizeof (tbuff));
    process_dbg_print_var (process, tbuff);
  }
  if (strcmp (tbuff, "printpath") == 0) {
    process_dbg_print_path (process);
  }
  if (strcmp (tbuff, "printinternal") == 0) {
    process_dbg_print_int_var (process);
  }
  if (strcmp (tbuff, "printinfo") == 0) {
    process_dbg_print_info (process);
  }

  return false;
}

void
process_stmt_executable (process_t *process, value_t *valnm)
{
  char            nm [MKC_VNAME_MAX];
  char            execnm [MKC_VNAME_MAX];
  listidx_t   siteridx;
  listidx_t   sidx;
  char            * epath;
  char            * tpath;
  char            * srcpath;
  bool            changed;
  mkc_alternate_t * curralt;


  process_check_mkc_timestamp (process);
  process_clean_check (process);

  sv_value_get_str (process->sv, valnm, nm, sizeof (nm));
  snprintf (execnm, sizeof (execnm), "%s%s", nm, process->exeext);

  epath = malloc (MKC_PATH_MAX);
  if (epath == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  *epath = '\0';

  path_build (MKC_PATH_STAGE_BIN, epath, MKC_PATH_MAX, execnm, process->mkcerr);
  tpath = strdup (epath);
  sv_set_str (process->sv, SV_T_PATHS, execnm, NULL, tpath, MKC_VCTXT_MKC);

  changed = process->mkc_changed;

  if (process->cacheloaded == false) {
    sv_set_integer (process->sv, SV_T_INTERNAL,
        MKC_C_MKC_CHANGED, NULL, true, MKC_VCTXT_MKC);
    changed = true;
    process->mkc_changed = true;
    process->reset_stage = true;
    process_clean_check (process);
  }

  if (! changed &&
      sv_is_defined (process->sv, SV_T_BUILD_DATA,
      epath, MKC_C_BVAR_DEPENDENCY)) {
    /* already in cache */
    free (epath);
    process_attr_clear (process);
    return;
  }

  srcpath = malloc (MKC_PATH_MAX);
  if (srcpath == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  *srcpath = '\0';

  list_iter_start (process->attr.sourcelist, &siteridx);
  while ((sidx = list_iter_next (process->attr.sourcelist, &siteridx)) != MKC_ITER_FINISH) {
    value_t     *src;
    char        objnm [MKC_VNAME_MAX];
    char        *p;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    src = list_get_by_idx (process->attr.sourcelist, sidx);
    sv_value_get_str (process->sv, src, srcpath, MKC_PATH_MAX);
    stpecpy (objnm, objnm + sizeof (objnm), srcpath);
    p = (char *) path_extension (objnm);
    if (p != NULL) {
      *p = '\0';
    }
    stpecpy (p, objnm + sizeof (objnm), process->objext);

    target_executable_object (process->target, execnm, objnm);
    target_object_source (process->target, objnm, srcpath);
  }

  curralt = process->attr.curralt;
  sv_set_list (process->sv, SV_T_BUILD_DATA, epath, MKC_C_BVAR_COMPFLAGS,
      curralt->compflags, MKC_VCTXT_MKC);
  sv_set_list (process->sv, SV_T_BUILD_DATA, epath, MKC_C_BVAR_LINKFLAGS,
      curralt->linkflags, MKC_VCTXT_MKC);
  sv_set_list (process->sv, SV_T_BUILD_DATA, epath, MKC_C_BVAR_LIBS,
      curralt->libs, MKC_VCTXT_MKC);

  free (srcpath);
  free (epath);
  process_attr_clear (process);
  return;
}

void
process_stmt_function_call (process_t *process,
    value_t *valparams, value_t *valfuncargs)
{
  list_t      *paramlist = NULL;
  list_t      *alist = NULL;
  listidx_t   aiteridx;
  listidx_t   nmiteridx;
  listidx_t   aidx;
  listidx_t   nmidx;

  sv_push (process->sv, SV_T_LOCAL, "local-function");

  if (valparams != NULL) {
    value_t   *value;

    value = sv_value_get_list_value (process->sv, valparams);
    if (value->vtype == MKC_VT_RANGE) {
      mkc_error_set (process->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT_TYPE, 0, NULL);
      return;
    }
    paramlist = value->list;
  }
  if (valfuncargs != NULL) {
    value_t   *value = NULL;
    value = sv_value_get_list_value (process->sv, valfuncargs);
    alist = value->list;
  }
  if ((alist == NULL && paramlist != NULL) ||
      (alist != NULL && paramlist == NULL) ||
      (alist != NULL &&
          list_size (alist) != list_size (paramlist))) {
    mkc_error_set (process->mkcerr, MKC_ERR_FUNCTION_ARG_MISMATCH, 0, NULL);
    return;
  }

  /* put the arguments into the local profile */
  list_iter_start (alist, &aiteridx);
  list_iter_start (paramlist, &nmiteridx);
  while ((aidx = list_iter_next (alist, &aiteridx)) != MKC_ITER_FINISH) {
    value_t     *aval;
    value_t     *nmval;

    nmidx = list_iter_next (paramlist, &nmiteridx);

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    aval = list_get_by_idx (alist, aidx);
    nmval = list_get_by_idx (paramlist, nmidx);
    process_local_set (process, nmval, aval);
  }
}

void
process_stmt_function_call_finish (process_t *process)
{
  sv_pop (process->sv);
}

void
process_stmt_loadcache (process_t *process, value_t *valvers)
{
  int     version;

  version = sv_value_get_integer (process->sv, valvers);
  if (version != MKC_CACHE_VERS_1) {
    mkc_message (MKC_V_BASIC, "-- cache version mismatch\n");
    process_attr_clear (process);
    return;
  }

  process->inloadcache = true;
  sv_set_fromcache (process->sv, true);

  process_attr_clear (process);
  return;
}

void
process_stmt_loadcache_post (process_t *process)
{
  bool        changed = false;

  process->cacheloaded = true;
  sv_set_fromcache (process->sv, false);

  if (process->cacheloaded && process->cacheinvalidated) {
    sv_reset (process->sv, process->mkcoptions);

    mkc_message (MKC_V_BASIC, "-- cache invalidated\n");
    mkc_log (process->log, MKC_LOG_GENERAL, "-- cache invalidated\n");
    process_set_defaults (process);
    process_initial_checks (process);
    process_get_path (process);
    process_find_executables (process);
    mkc_chk_getconf (process->check);

    changed = true;
    process->mkc_changed = true;
    process->reset_stage = true;
  }

  process_check_mkc_timestamp (process);
  process_clean_check (process);

  /* the changed flag will invalidate certain cached items */
  sv_set_integer (process->sv, SV_T_INTERNAL,
        MKC_C_MKC_CHANGED, NULL, changed, MKC_VCTXT_MKC);

  process->inloadcache = false;

  process_attr_clear (process);
  return;
}

void
process_stmt_mark (process_t *process,
    value_t *vala, value_t *valb)
{
  char    nm [MKC_VNAME_MAX];
  char    val [MKC_VNAME_MAX];

  if (process == NULL) {
    return;
  }

  sv_value_get_str (process->sv, vala, nm, sizeof (nm));
  sv_value_get_str (process->sv, valb, val, sizeof (val));
  if (*nm == '\0') {
    mkc_error_set (process->mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
    process_attr_clear (process);
    return;
  }
  if (strcmp (val, "disable-output") == 0 ||
      strcmp (val, "disable") == 0) {
    sv_set_context (process->sv, nm, MKC_VCTXT_USER_DISABLE);
  } else if (strcmp (val, "enable-output") == 0 ||
      strcmp (val, "enable") == 0) {
    sv_set_context (process->sv, nm, MKC_VCTXT_USER_ENABLE);
  } else {
    mkc_error_set (process->mkcerr, MKC_ERR_PROC_INVALID_MARK, 0, NULL);
  }

  process_attr_clear (process);
  return;
}

void
process_stmt_print (process_t *process, value_t *value, int depth)
{
  char      tbuff [MKC_PATH_MAX];

  if (process == NULL) {
    return;
  }
  if (value == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  sv_value_get_str (process->sv, value, tbuff, sizeof (tbuff));
  fprintf (stdout, "%s", tbuff);

  if (depth == 0) {
    fprintf (stdout, "\n");
    fflush (stdout);
  }
}

void
process_stmt_profile (process_t *process, value_t *valnm)
{
  char        nm [MKC_VNAME_MAX];

  sv_value_get_str (process->sv, valnm, nm, sizeof (nm));
  sv_set_active_profile (process->sv, nm);
  /* if a compiler is set, it has not yet been processed */
}

void
process_stmt_profile_post (process_t *process)
{
  sv_reset_profile (process->sv);
}

void
process_stmt_project (process_t *process, value_t *valnm)
{
  char            projnm [MKC_VNAME_MAX];

  sv_value_get_str (process->sv, valnm, projnm, sizeof (projnm));

  datafree (process->projectname);
  process->projectname = strdup (projnm);
  path_set_dir_relative (MKC_DIR_PROJECT, projnm);
  process->dfltcompiler = process->attr.currcompiler;

  sv_set_str (process->sv, SV_T_INTERNAL,
      MKC_C_PROJECT_NAME, NULL, process->projectname, MKC_VCTXT_MKC_BASE);
  if (process->attr.str [MKC_ATTR_VERSION] != NULL) {
    sv_set_str (process->sv, SV_T_INTERNAL,
        MKC_C_PROJECT_VERS, NULL, process->attr.str [MKC_ATTR_VERSION], MKC_VCTXT_MKC_BASE);
  }
  if (process->attr.str [MKC_ATTR_LIB_VERSION] != NULL) {
    sv_set_str (process->sv, SV_T_INTERNAL,
        MKC_C_PROJECT_LIB_VERS, NULL, process->attr.str [MKC_ATTR_LIB_VERSION], MKC_VCTXT_MKC_BASE);
  }

  process_check_mkc_timestamp (process);
  process_clean_check (process);

  process_attr_clear (process);
  return;
}

int
process_stmt_set (process_t *process,
    value_t *valnm, value_t *value, bool local)
{
  char            *nm;
  value_t         rvalue;
  value_t         * tvalue = &rvalue;
  mkc_err_code_t  trc = MKC_ERR_FAILURE;
  value_ctxt_t    vctxt = MKC_VCTXT_USER_DISABLE;
  bool            istempval = false;
  bool            changed = false;
  sv_type_t       svtype;

  if (process == NULL) {
    return trc;
  }
  if (valnm == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    process_attr_clear (process);
    return trc;
  }

  changed = process->mkc_changed;

  /* internal names can be quite long */
  nm = malloc (MKC_PATH_MAX);
  if (nm == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return trc;
  }

  sv_value_get_str (process->sv, valnm, nm, MKC_PATH_MAX);
  if (mkc_error_chk_err (process->mkcerr)) {
    process_attr_clear (process);
    free (nm);
    return trc;
  }
  if (*nm == '\0') {
    mkc_error_set (process->mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
    process_attr_clear (process);
    free (nm);
    return trc;
  }

  sv_value_get_value (process->sv, value, tvalue);
  if (mkc_error_chk_err (process->mkcerr)) {
    process_attr_clear (process);
    free (nm);
    return trc;
  }
  istempval = tvalue->isallocated;

  if (process->attr.str [MKC_ATTR_VCONTEXT] != NULL) {
    const char    *tvc;

    tvc = process->attr.str [MKC_ATTR_VCONTEXT];
    vctxt = value_ctxt_value (tvc);
    if (vctxt == MKC_VCTXT_UNKNOWN) {
      if (strcmp (tvc, "disable-output") == 0) {
        vctxt = MKC_VCTXT_USER_DISABLE;
      } else if (strcmp (tvc, "enable-output") == 0) {
        vctxt = MKC_VCTXT_USER_ENABLE;
      }
    }
  }

  svtype = SV_T_SEARCH;
  if (local) {
    svtype = SV_T_LOCAL;
  }

  trc = sv_set (process->sv, svtype, nm, NULL, tvalue, vctxt);
  if (trc == MKC_OK_CHANGE &&
     (vctxt == MKC_VCTXT_ENV || vctxt == MKC_VCTXT_MKC_BASE)) {
    process->cacheinvalidated = true;
    process->reset_stage = true;
    changed = true;
    process->mkc_changed = true;
  } else if (trc == MKC_OK_CHANGE && strcmp (nm, MKC_C_PREFIX) == 0) {
    changed = true;
    process->mkc_changed = true;
    process->reset_stage = true;
    process_clean_check (process);
    trc = MKC_OK;
  } else {
    trc = MKC_OK;
  }

  sv_set_integer (process->sv, SV_T_INTERNAL,
        MKC_C_MKC_CHANGED, NULL, changed, MKC_VCTXT_MKC);

  /* tvalue may have been re-allocated, only call temp-value-free */
  /* if the tvalue was allocated */
  if (istempval) {
    value_free (tvalue);
  }

  process_attr_clear (process);
  free (nm);
  return trc;
}

/* attributes */

void
process_attribute (process_t *process, value_t *valname,
    astnode_token_t asttype)
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
          MKC_CONTEXT_ALTERNATE | MKC_CONTEXT_EXECUTABLE;
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
    sv_value_get_str (process->sv, valname, nm, sizeof (nm));
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
process_attr_alternate (process_t *process)
{
  mkc_alternate_t   alt;

  alt.name = NULL;
  alt.hdrlist = list_init (MKC_LIST_UNSORTED, NULL, NULL, sizeof (value_t), process->mkcerr);
  alt.compflags = list_init (MKC_LIST_UNSORTED, NULL, NULL, sizeof (value_t), process->mkcerr);
  alt.linkflags = list_init (MKC_LIST_UNSORTED, NULL, NULL, sizeof (value_t), process->mkcerr);
  alt.libs = list_init (MKC_LIST_UNSORTED, NULL, NULL, sizeof (value_t), process->mkcerr);
  process->attr.curralt = list_set (process->attr.alternates, &alt);
}

void
process_attr_compiler (process_t *process, value_t *name)
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

  sv_value_get_str (process->sv, name, nm, sizeof (nm));
  if (mkc_context_check (process->context, MKC_CONTEXT_PROJECT)) {
    /* if in a project statement, the default compiler is set */
    process->dfltcompiler = compiler_get_id (nm);
    sv_set_default_compiler (process->sv, process->dfltcompiler);
  }

  process->attr.currcompiler = compiler_get_id (nm);

  if (mkc_context_check (process->context, MKC_CONTEXT_PROFILE)) {
    sv_set_current_compiler (process->sv, process->attr.currcompiler);
  }
}

void
process_attr_comp_flags (process_t *process, value_t *value)
{
  list_t      * clist;

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
  process_list_to_flags (process, value, clist, false);
}

void
process_attr_header (process_t *process, value_t *value)
{
  listidx_t   iteridx;
  listidx_t   lidx;
  list_t      * hlist;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context,
      MKC_CONTEXT_CHECK | MKC_CONTEXT_ALTERNATE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  hlist = process->attr.curralt->hdrlist;

  list_iter_start (value->list, &iteridx);
  while ((lidx = list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    value_t     *lvalue;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = list_get_by_idx (value->list, lidx);
    list_set (hlist, lvalue);
  }

  return;
}

void
process_attr_link_flags (process_t *process, value_t *value)
{
  list_t      * llist;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context,
      MKC_CONTEXT_CHECK | MKC_CONTEXT_ALTERNATE | MKC_CONTEXT_EXECUTABLE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  llist = process->attr.curralt->linkflags;
  process_list_to_flags (process, value, llist, false);
}

void
process_attr_lib_flags (process_t *process, value_t *value)
{
  list_t      * libs;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context,
      MKC_CONTEXT_CHECK | MKC_CONTEXT_ALTERNATE | MKC_CONTEXT_EXECUTABLE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  libs = process->attr.curralt->libs;
  process_list_to_flags (process, value, libs, false);
}

void
process_attr_path (process_t *process, value_t *path)
{
  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context,
      MKC_CONTEXT_CHECK | MKC_CONTEXT_CHK_INC)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  list_set (process->attr.pathlist, path);
  return;
}

void
process_attr_replace (process_t *process,
    value_t *str, value_t *name)
{
  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context, MKC_CONTEXT_CONFIGURE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  list_set (process->attr.replacelist, str);
  list_set (process->attr.replacelist, name);
  return;
}

void
process_attr_source (process_t *process, value_t *value)
{
  listidx_t   iteridx;
  listidx_t   lidx;
  list_t      * srclist;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context, MKC_CONTEXT_EXECUTABLE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  srclist = process->attr.sourcelist;

  list_iter_start (value->list, &iteridx);
  while ((lidx = list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    value_t     *lvalue;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = list_get_by_idx (value->list, lidx);
    list_set (srclist, lvalue);
  }

  return;
}


int32_t
process_check (process_t *process, value_t *valconst,
    astnode_token_t asttype)
{
  int         rc = MKC_OK;
  char        tnm [MKC_VNAME_MAX];
  char        txt [MKC_VNAME_MAX];
  char        pfx [MKC_VNAME_MAX];
  scopedvar_t * scope;
  int         iasttype = asttype;
  bool        successtype = false;
  bool        valtype = false;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  scope = process->sv;
  sv_value_get_str (scope, valconst, txt, sizeof (txt));
  snprintf (pfx, sizeof (pfx), "_%s_", typenames [asttype]);
  process_create_name (process, asttype, tnm, sizeof (tnm), pfx, txt, NULL);

  if (process_chk_cache (process, txt, tnm)) {
    value_t   * value;

    value = sv_get_value (scope, SV_T_SEARCH, tnm, NULL);
    rc = sv_value_get_integer (scope, value);
    switch (iasttype) {
      case MKC_T_CHK_ARG_COUNT:
      case MKC_T_CHK_SIZE: {
        break;
      }
      default: {
        if (rc == 0) {
          rc = MKC_ERR_FAILURE;
        } else {
          rc = MKC_OK;
        }
        break;
      }
    }
    process_attr_clear (process);
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

    sv_set_integer (scope, SV_T_SEARCH, tnm, NULL, rc == 0 ? true : false, MKC_VCTXT_CHECK);
    mkc_message (MKC_V_BASIC, "-- check %s: %s : %s - %s\n",
        typenames [asttype], txt, tnm, mkc_success_msg (rc));
    mkc_log (process->log, MKC_LOG_CHECK, "-- check %s: %s : %s - %s\n",
        typenames [asttype], txt, tnm, mkc_success_msg (rc));
  }
  if (valtype) {
    /* the check is run, and the return code is a value */
    sv_set_integer (scope, SV_T_SEARCH, tnm, NULL, rc, MKC_VCTXT_CHECK);
    mkc_message (MKC_V_BASIC, "-- check %s: %s : %s : %d\n", typenames [asttype], txt, tnm, rc);
    mkc_log (process->log, MKC_LOG_CHECK,
        "-- check %s: %s : %s : %d\n", typenames [asttype], txt, tnm, rc);
  }

  process_attr_clear (process);
  return rc;
}

int32_t
process_check_flag (process_t *process,
    value_t *valflag, int addchk, astnode_token_t asttype)
{
  int         rc = MKC_OK;
  char        tnm [MKC_VNAME_MAX];
  char        flag [MKC_VNAME_MAX];
  const char  *pfx = NULL;
  scopedvar_t *scope;
  int         iasttype = asttype;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  scope = process->sv;
  sv_value_get_str (scope, valflag, flag, sizeof (flag));

  if (! *flag) {
    /* empty flags are ignored */
    return MKC_ERR_FAILURE;
  }

  switch (iasttype) {
    case MKC_T_CHK_COMP_FLAG: { pfx = "cf_"; break; }
    case MKC_T_CHK_LINK_FLAG: { pfx = "lf_"; break; }
    case MKC_T_CHK_LIBRARY: { pfx = "lib_"; break; }
  }
  process_create_name (process, asttype, tnm, sizeof (tnm), pfx, flag, NULL);

  if (process_chk_cache (process, flag, tnm)) {
    process_attr_clear (process);
    return MKC_OK;
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
    sv_set_str (scope, SV_T_SEARCH, tnm, NULL, flag, MKC_VCTXT_FLAG);

    switch (iasttype) {
      case MKC_T_CHK_COMP_FLAG: {
        sv_append_str_list (process->sv, SV_T_ACTIVE,
            MKC_C_CFLAGS, NULL, flag, MKC_VCTXT_MKC);
        break;
      }
      case MKC_T_CHK_LIBRARY: {
        sv_append_str_list (process->sv, SV_T_ACTIVE,
            MKC_C_LIBS, NULL, flag, MKC_VCTXT_MKC);
        break;
      }
      case MKC_T_CHK_LINK_FLAG: {
        const char    *nm = MKC_C_LDFLAGS;

        if (mkc_flag_is_libloc (process->compid, flag) ||
            strncmp (flag,
            compiler_get_flag (process->compid, MKC_COMP_FLAG_LIB),
            compiler_get_flag_len (process->compid, MKC_COMP_FLAG_LIB)) == 0) {
          nm = MKC_C_LIBS;
        }
        sv_append_str_list (process->sv, SV_T_ACTIVE,
            nm, NULL, flag, MKC_VCTXT_MKC);
        break;
      }
    }
  }

  if (addchk == MKC_ADD) {
    mkc_message (MKC_V_BASIC, "-- add %s: %s\n", typenames [asttype], flag);
    mkc_log (process->log, MKC_LOG_CHECK,
        "-- add %s: %s\n", typenames [asttype], flag);
  }
  if (addchk == MKC_CHK) {
    mkc_message (MKC_V_BASIC, "-- check %s: %s - %s\n",
        typenames [asttype], flag, mkc_success_msg (rc));
    mkc_log (process->log, MKC_LOG_CHECK, "-- check %s: %s - %s\n",
        typenames [asttype], flag, mkc_success_msg (rc));
  }

  process_attr_clear (process);
  return rc;
}

int32_t
process_chk_struct_member (process_t *process,
    value_t *valstructnm, value_t *valmembernm)
{
  int         rc = MKC_OK;
  char        tnm [MKC_VNAME_MAX];
  char        structname [MKC_VNAME_MAX];
  char        membername [MKC_VNAME_MAX];
  scopedvar_t * scope;
  char        tmpdisp [MKC_VNAME_MAX * 2];

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  scope = process->sv;
  sv_value_get_str (scope, valstructnm, structname, sizeof (structname));
  sv_value_get_str (scope, valmembernm, membername, sizeof (membername));
  process_create_name (process, MKC_T_CHK_STRUCT_MEMBER, tnm, sizeof (tnm),
      "_member_", structname, membername, NULL);

  snprintf (tmpdisp, sizeof (tmpdisp), "%s.%s", structname, membername);
  if (process_chk_cache (process, tmpdisp, tnm)) {
    value_t   * value;

    value = sv_get_value (scope, SV_T_SEARCH, tnm, NULL);
    rc = sv_value_get_integer (scope, value);
    if (rc == 0) {
      rc = MKC_ERR_FAILURE;
    } else {
      rc = MKC_OK;
    }
    process_attr_clear (process);
    return rc;
  }

  rc = mkc_chk_struct_member (process->check,
      process->attr.currcompiler, structname, membername);
  sv_set_integer (scope, SV_T_SEARCH,
      tnm, NULL, rc == 0 ? true : false, MKC_VCTXT_CHECK);

  mkc_message (MKC_V_BASIC, "-- check struct member: %s.%s - %s\n",
      structname, membername, mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check struct member: %s.%s - %s\n",
      structname, membername, mkc_success_msg (rc));

  process_attr_clear (process);
  return rc;
}

int
process_chk_shell_extract (process_t *process, value_t *valpath)
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
  *path = '\0';

  sv_value_get_str (process->sv, valpath, path, MKC_PATH_MAX);

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
  *varvalue = '\0';

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
    tvalue = sv_substitute (process->sv, varvalue, SV_SUB_ESCAPE, 0);
    if (tvalue == NULL) {
      continue;
    }

    sv_set_str (process->sv, SV_T_SEARCH, varname, NULL, tvalue, MKC_VCTXT_CHECK);

    mkc_message (MKC_V_BASIC, "-- shell extract %s %s\n", varname, tvalue);
    mkc_log (process->log, MKC_LOG_CHECK, "-- shell extract %s %s\n",
        varname, tvalue);

    mkc_regex_get_free (match);
    free (tvalue);
  }

  free (path);
  free (varvalue);
  datafree (buff);
#endif

  process_attr_clear (process);
  return MKC_OK;
}

void
process_local_set (process_t *process, value_t *nmval,
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

  sv_value_get_str (process->sv, nmval, nm, sizeof (nm));

  sv_set (process->sv, SV_T_LOCAL, nm, NULL, argval, MKC_VCTXT_TEMP);
}

int32_t
process_get_loop_limit (process_t *process)
{
  int32_t   limit = 10000;
  value_t   *value;

  if (process == NULL) {
    return limit;
  }

  value = sv_get_value (process->sv, SV_T_INTERNAL, MKC_C_LOOPLIMIT, NULL);
  if (value != NULL) {
    limit = sv_value_get_integer (process->sv, value);
  }

  return limit;
}

void
process_save_cache (process_t *process)
{
  scopedvar_t     * sv;
  char            * cachename;
  const char      * profname;
  FILE            * fh;
  int             tcount = 0;
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
  *cachename = '\0';

  path_build (MKC_PATH_MKCFILES, cachename, MKC_PATH_MAX,
      "cache.mkc", process->mkcerr);
  fh = fileop_open (cachename, "w");
  if (fh == NULL) {
    free (cachename);
    return;
  }

  sv = process->sv;

  /* version 1 */
  fprintf (fh, "load_cache %d {\n", MKC_CACHE_VERS_1);

  sviter = sv_iter_start (sv, SV_ITER_PROFILES);
  while ((profname = sv_iter_next (sv, sviter)) != NULL) {
    if (mkc_error_chk_err (process->mkcerr)) {
      return;
    }

    process_save_cache_profile (process, fh, sviter, profname, &tcount);
  }
  sv_iter_finish (sviter);

  if (tcount == 0) {
    fprintf (fh, "  ;\n");
  }
  fprintf (fh, "}\n");

  free (cachename);
  fclose (fh);
}

bool
process_profile_is_current (process_t *process, value_t *valnm)
{
  char        nm [MKC_VNAME_MAX];
  const char  *profnm;

  if (process->inloadcache) {
    return true;
  }

  sv_value_get_str (process->sv, valnm, nm, sizeof (nm));
  profnm = sv_get_current_profile (process->sv);

  if (strcmp (nm, profnm) == 0 ||
      strcmp (nm, MKC_C_PROF_NAME_INTERNAL) == 0 ||
      strcmp (nm, MKC_C_PROF_NAME_DEFAULT) == 0) {
    return true;
  }

  return false;
}

/* internal routines */

const char *
process_create_name (process_t *process, astnode_token_t asttype,
    char *buff, size_t sz, const char *tag, ...)
{
  char            *p;
  size_t          len;
  size_t          nlen = 0;
  const char      * str;
  va_list         ap;
  mkc_alternate_t * alt;
  listidx_t   iteridx;
  listidx_t   aidx;

  va_start (ap, tag);

  /* get the first alternate in the list */
  /* curralt is pointing to the last */
  /* the name of the check comes from the first alternate, */
  /* which has the settings of the base test */
  list_iter_start (process->attr.alternates, &iteridx);
  aidx = list_iter_next (process->attr.alternates, &iteridx);
  alt = list_get_by_idx (process->attr.alternates, aidx);

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
process_initial_checks (process_t *process)
{
  int       rc;
  int       isystype;
  char    tbuff [MKC_PATH_MAX];

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

  /* compiler id : internal */

  rc = mkc_chk_compiler_id (process->check, process->dfltcompiler);
  if (rc >= 0) {
    process->compid = rc;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "compiler-id", process->compid);

  for (mkc_compiler_id_t i = 0; i < MKC_COMP_ID_MAX; ++i) {
    if (process->compid == i) {
      sv_set_integer (process->sv, SV_T_INTERNAL, compidnames [i], NULL, true, MKC_VCTXT_MKC_BASE);
      break;
    }
  }

  /* modern header support : dflt/comp */

  rc = mkc_chk_header_modern (process->check, process->dfltcompiler);
  if (rc != 0) {
    process->headertype = MKC_HEADER_LEGACY;
  }
  process->attr.headertype = process->headertype;
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "header-type", process->headertype);

  /* system type : internal */

  rc = mkc_chk_system_type (process->check, process->dfltcompiler);
  if (rc >= 0) {
    process->systype = rc;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "system-type", process->systype);

  for (mkc_system_type_t i = 0; i < MKC_SYS_MAX; ++i) {
    if (process->systype == i) {
      sv_set_integer (process->sv, SV_T_INTERNAL, sysnames [i], NULL, true, MKC_VCTXT_MKC_BASE);
      break;
    }
  }

  /* object, executable extension : internal */

  if (process->systype == MKC_SYS_WINDOWS) {
    process->objext = ".obj";
    process->exeext = ".exe";
  }
  sv_set_str (process->sv, SV_T_INTERNAL,
      MKC_C_OBJEXT, NULL, process->objext, MKC_VCTXT_MKC_BASE);
  sv_set_str (process->sv, SV_T_INTERNAL,
      MKC_C_EXEEXT, NULL, process->exeext, MKC_VCTXT_MKC_BASE);

  /* shared library extension : internal */

  /* default is .so */
  sv_set_str (process->sv, SV_T_INTERNAL,
      MKC_C_SHLIBEXT, NULL, ".so", MKC_VCTXT_MKC_BASE);
  isystype = process->systype;
  switch (isystype) {
    case MKC_SYS_AIX: {
      sv_set_str (process->sv, SV_T_INTERNAL,
          MKC_C_SHLIBEXT, NULL, ".a", MKC_VCTXT_MKC_BASE);
      break;
    }
    case MKC_SYS_MACOS: {
      sv_set_str (process->sv, SV_T_INTERNAL,
          MKC_C_SHLIBEXT, NULL, ".dylib", MKC_VCTXT_MKC_BASE);
      break;
    }
    case MKC_SYS_WINDOWS: {
      sv_set_str (process->sv, SV_T_INTERNAL,
          MKC_C_SHLIBEXT, NULL, ".dll", MKC_VCTXT_MKC_BASE);
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
      sv_set_integer (process->sv, SV_T_INTERNAL,
          sysidnames [i], NULL, true, MKC_VCTXT_MKC_BASE);
      break;
    }
  }

  /* linux: library location : internal */

  if (process->systype == MKC_SYS_LINUX) {
    rc = mkc_chk_library_location (process->check, process->dfltcompiler);
    if (rc >= 0) {
      process->libloc = rc;
    }
    mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", MKC_C_LIBLOCNAME, process->libloc);
    sv_set_integer (process->sv, SV_T_INTERNAL,
        MKC_C_LIBLOCNAME, NULL, process->libloc, MKC_VCTXT_MKC_BASE);
  }

  /* check if compiler supports the -MM flag */

  rc = mkc_chk_compiler_flag (process->check,
      process->attr.currcompiler,
      compiler_get_flag (process->compid, MKC_COMP_FLAG_DEPS_USER), false);
  if (rc == MKC_OK) {
    mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", MKC_C_SUPPORTS_MM, process->libloc);
    process->compiler_mm = true;
  }
  sv_set_integer (process->sv, SV_T_INTERNAL,
      MKC_C_SUPPORTS_MM, NULL, process->compiler_mm, MKC_VCTXT_MKC_BASE);

  /* variadic macro support : dflt/comp */

  rc = mkc_chk_variadic_macro (process->check, process->dfltcompiler);
  if (rc != 0) {
    process->variadicmacro = MKC_NO_VARIADIC_MACRO;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", MKC_C_IVARMACRO, process->variadicmacro);
  sv_set_integer (process->sv, SV_T_SEARCH,
      MKC_C_IVARMACRO, NULL, process->variadicmacro, MKC_VCTXT_MKC);

  /* make sure these variables exist */
  sv_set_integer (process->sv, SV_T_INTERNAL,
        MKC_C_MKC_CHANGED, NULL, false, MKC_VCTXT_MKC);

  path_build (MKC_PATH_PREFIX, tbuff, sizeof (tbuff), NULL, process->mkcerr);
  sv_set_str (process->sv, SV_T_INTERNAL,
      MKC_C_PREFIX, NULL, tbuff, MKC_VCTXT_MKC);

  process_attr_clear (process);
  return MKC_OK;
}

static void
process_set_defaults (process_t *process)
{
  dict_t    *dict;

  /* create internal constants */

  for (mkc_system_type_t i = 0; i < MKC_SYS_MAX; ++i) {
    sv_set_integer (process->sv, SV_T_INTERNAL,
        sysnames [i], NULL, false, MKC_VCTXT_MKC_BASE);
  }
  for (mkc_system_id_t i = 0; i < MKC_SYS_ID_MAX; ++i) {
    sv_set_integer (process->sv, SV_T_INTERNAL,
        sysidnames [i], NULL, false, MKC_VCTXT_MKC_BASE);
  }

  sv_set_integer (process->sv, SV_T_INTERNAL,
      MKC_C_LOOPLIMIT, NULL, 10000, MKC_VCTXT_MKC);
  sv_set_integer (process->sv, SV_T_INTERNAL,
      MKC_C_LIBLOCNAME, NULL, process->libloc, MKC_VCTXT_MKC_BASE);

  sv_set_str (process->sv, SV_T_INTERNAL,
      "BISON", NULL, "bison", MKC_VCTXT_ENV);
  sv_set_str (process->sv, SV_T_INTERNAL,
      "CC", NULL, "cc", MKC_VCTXT_ENV);
  sv_set_str (process->sv, SV_T_INTERNAL,
      "CXX", NULL, "c++", MKC_VCTXT_ENV);
  sv_set_str (process->sv, SV_T_INTERNAL,
      "DC", NULL, "dc", MKC_VCTXT_ENV);
  sv_set_str (process->sv, SV_T_INTERNAL,
      "FLEX", NULL, "flex", MKC_VCTXT_ENV);
  sv_set_str (process->sv, SV_T_INTERNAL,
      "OBJC", NULL, "cc", MKC_VCTXT_ENV);

  for (mkc_compiler_id_t i = 0; i < MKC_COMP_ID_MAX; ++i) {
    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }
    sv_set_integer (process->sv, SV_T_INTERNAL,
        compidnames [i], NULL, false, MKC_VCTXT_MKC_BASE);
  }

  sv_set_str (process->sv, SV_T_INTERNAL,
      MKC_C_PROFILE_NAME, NULL,
      sv_get_current_profile (process->sv), MKC_VCTXT_MKC);

  dict = dict_init (process->log, value_free, sizeof (value_t), process->mkcerr);
  sv_set_dict (process->sv, SV_T_INTERNAL, MKC_C_VAR_BUILD_PATHS, dict, MKC_VCTXT_MKC);
  sv_set_dict (process->sv, SV_T_INTERNAL, MKC_C_VAR_BUILD_DATA, dict, MKC_VCTXT_MKC);
  dict_free (dict);
}

static void
process_configure_manual (process_t *process)
{
  char    *data;
  char    *ndata;
  size_t  fsz = 0;
  FILE    *fh;

  mkc_message (MKC_V_BASIC, "-- configure: create: %s\n",
      process->attr.str [MKC_ATTR_OUTPUT]);

  data = fileop_read_file (process->attr.str [MKC_ATTR_INPUT], &fsz, process->mkcerr);
  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_error_set (process->mkcerr, MKC_ERR_FILE_NOT_FOUND,
        errno, process->attr.str [MKC_ATTR_INPUT]);
    return;
  }
  ndata = process_configure_substitute (process, data);
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
process_configure_auto (process_t *process, int defzero)
{
  FILE            * fh;
  char            * fname;
  char            * tbuff;
  char            * tp;
  char            autooutnm [MKC_VNAME_MAX];
  size_t          len;
  scopedvar_t     * sv;
  sv_iter_t       * sviter;
  const char      * profname;
  const char      * currprof;

  fname = malloc (MKC_PATH_MAX);
  if (fname == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  *fname = '\0';

  tp = process->projectname;
  if (tp == NULL) {
    tp = "project";
  }
  tp = stpecpy (autooutnm, autooutnm + sizeof (autooutnm), tp);
  tp = stpecpy (tp, autooutnm + sizeof (autooutnm), "_config");

  if (process->attr.str [MKC_ATTR_OUTPUT] != NULL) {
    stpecpy (fname, fname + MKC_PATH_MAX, process->attr.str [MKC_ATTR_OUTPUT]);
    tp = strrchr (fname, '/');
    if (tp != NULL) {
      tp = stpecpy (autooutnm, autooutnm + sizeof (autooutnm), tp + 1);
      tp = strrchr (autooutnm, '.');
      if (tp != NULL) {
        *tp = '\0';
      }
    }
  } else {
    snprintf (fname, MKC_PATH_MAX, "%s.h", autooutnm);
  }

  len = strlen (autooutnm);
  for (size_t i = 0; i < len; ++i) {
    if (! isalnum ((unsigned char) autooutnm [i])) {
      autooutnm [i] = '_';
    } else {
      autooutnm [i] = toupper (autooutnm [i]);
    }
  }

  mkc_message (MKC_V_BASIC, "-- configure: create: %s\n", fname);

  fh = fileop_open (fname, "w");
  if (fh == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, fname);
    return;
  }

  fprintf (fh, "/* built by mkc */\n");
  fprintf (fh, "#ifndef INC_%s_H\n", autooutnm);
  fprintf (fh, "#define INC_%s_H\n", autooutnm);
  fprintf (fh, "\n");

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    free (fname);
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  *tbuff = '\0';

  sv = process->sv;

  currprof = sv_get_current_profile (sv);

  sviter = sv_iter_start (sv, SV_ITER_PROFILES);
  while ((profname = sv_iter_next (sv, sviter)) != NULL) {
    mkc_varidx_t    viter;
    mkc_varidx_t    vidx;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    if (strcmp (profname, MKC_C_PROF_NAME_INTERNAL) != 0 &&
        strcmp (profname, MKC_C_PROF_NAME_DEFAULT) != 0 &&
        strcmp (profname, currprof) != 0) {
      /* only process internal, default and the user selected profile */
      continue;
    }

    sv_var_iter_start (sv, sviter, &viter);
    while ((vidx = sv_var_iter_next (sv, sviter, &viter)) != MKC_ITER_FINISH) {
      const char  * nm;
      value_t     * value;

      nm = sv_var_iter_get_name (sv, sviter, vidx);
      value = sv_var_iter_get_value (sv, sviter, vidx);
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
        int64_t    tmval;

        tmval = value->tmval;
        if (defzero == MKC_AUTO_DEFINE_ZERO || tmval != 0) {
          fprintf (fh, "#define %s %" PRId64 "\n", nm, tmval);
        }
      } else {
        value_to_str (value, tbuff, MKC_PATH_MAX, 0);
        fprintf (fh, "#define %s \"%s\"\n", nm, tbuff);
      }
    }
  }
  sv_iter_finish (sviter);

  fprintf (fh, "\n");
  fprintf (fh, "#endif /* INC_%s_H */\n", autooutnm);

  free (fname);
  free (tbuff);
  fclose (fh);
}

static bool
process_chk_cache (process_t *process,
    const char *disp, const char *nm)
{
  bool    rc = false;

  /* if the re-test mkc-option is set, then failed tests will be re-tested */
  if (sv_is_defined (process->sv, SV_T_SEARCH, nm, NULL)) {
    if (process->mkcoptions->retest) {
      value_t   *value;

      value = sv_get_value (process->sv, SV_T_SEARCH, nm, NULL);
      if (value->vtype == MKC_VT_INTEGER) {
        int32_t   val;

        val = sv_value_get_integer (process->sv, value);
        if (! val) {
          return rc;
        }
      }
    }

    mkc_message (MKC_V_BASIC, "-- cached: %s : %s\n", disp, nm);
    mkc_log (process->log, MKC_LOG_CHECK, "-- cached: %s : %s\n", disp, nm);
    rc = true;
  }

  return rc;
}

static void
process_get_path (process_t *process)
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
  *tbuff = '\0';

  env_get ("PATH", tbuff, MKC_SMALL_BUFF_SZ);

  tpath = str_token (tbuff, pathdelim, &tokstr);
  while (tpath != NULL) {
    fileop_normalize_path (tpath, strlen (tpath));
    sv_append_str_list (process->sv, SV_T_INTERNAL,
        MKC_C_PATH, NULL, tpath, MKC_VCTXT_MKC);
    tpath = str_token (NULL, pathdelim, &tokstr);
  }

  free (tbuff);
}

static void
process_find_executables (process_t *process)
{
  char            * testpath;
  mkc_prog_chk_t  * chk;
  char            * p;
  list_t          * pathlist;
  listidx_t       iteridx;
  listidx_t       lidx;
  value_t         *valpath;


  testpath = malloc (MKC_PATH_MAX);
  if (testpath == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  *testpath = '\0';

  valpath = sv_get_value (process->sv, SV_T_INTERNAL, MKC_C_PATH, NULL);
  pathlist = valpath->list;

  chk = proglist;
  while (chk->program != NULL) {
    list_iter_start (pathlist, &iteridx);
    while ((lidx = list_iter_next (pathlist, &iteridx)) != MKC_ITER_FINISH) {
      value_t   *lvalue;

      lvalue = list_get_by_idx (pathlist, lidx);
      sv_value_get_str (process->sv, lvalue, testpath, MKC_PATH_MAX);

      p = testpath + strlen (testpath);
      p = stpecpy (p, testpath + MKC_PATH_MAX, "/");
      p = stpecpy (p, testpath + MKC_PATH_MAX, chk->program);
      p = stpecpy (p, testpath + MKC_PATH_MAX, process->exeext);

      if (fileop_exists (testpath)) {
        sv_set_str (process->sv, SV_T_INTERNAL,
            chk->mkcvarname, NULL, testpath, MKC_VCTXT_MKC);
        break;
      }
    }
    chk += 1;
  }

  free (testpath);
}

static void
process_attr_clear (process_t *process)
{
  list_free (process->attr.alternates);
  process->attr.alternates = list_init (MKC_LIST_UNSORTED,
      process_alternate_free, NULL, sizeof (mkc_alternate_t), process->mkcerr);
  process_attr_alternate (process);

  if (list_size (process->attr.pathlist) > 0) {
    list_free (process->attr.pathlist);
    process->attr.pathlist = list_init (MKC_LIST_UNSORTED, NULL, NULL,
        sizeof (value_t), process->mkcerr);
  }

  if (list_size (process->attr.replacelist) > 0) {
    list_free (process->attr.replacelist);
    process->attr.replacelist = list_init (MKC_LIST_UNSORTED, NULL, NULL,
        sizeof (value_t), process->mkcerr);
  }

  for (int i = 0; i < MKC_ATTR_MAX; ++i) {
    datafree (process->attr.str [i]);
    process->attr.str [i] = NULL;
  }

  process->attr.define_zero = MKC_AUTO_SKIP_ZERO;
  process->attr.currcompiler = process->dfltcompiler;
  process->attr.localheader = false;
  process->attr.display = false;
  process->attr.printerrors = false;
  process->attr.negate = false;
}

static mkc_user_regex_t *
process_user_regex_init (process_t *process, const char *pattern)
{
  mkc_user_regex_t    *urx;
  mkc_user_regex_t    turx;
  listidx_t       idx;

  turx.rx = NULL;
  turx.pattern = (char *) pattern;

  idx = list_find (process->user_rx_list, &turx);
  if (idx != MKC_LIST_NOTFOUND) {
    urx = list_get_by_idx (process->user_rx_list, idx);
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
  urx = list_set (process->user_rx_list, &turx);

  return urx;
}

static void
process_user_regex_free (void *turx)
{
  mkc_user_regex_t  *urx = turx;

  datafree (urx->pattern);
#if _have_regex
  mkc_regex_free (urx->rx);
#endif
}

static int
process_user_regex_comp (void *turxa, void *turxb)
{
  mkc_user_regex_t  *urxa = turxa;
  mkc_user_regex_t  *urxb = turxb;

  return strcmp (urxa->pattern, urxb->pattern);
}

static char *
process_configure_substitute (process_t *process, char *data)
{
  char          *ndata = NULL;
  list_t    *rl;

  rl = process->attr.replacelist;
  if (list_size (rl) == 0) {
    ndata = sv_substitute (process->sv, data, SV_NO_ESCAPE, 0);
  } else {
    listidx_t   iteridx;
    listidx_t   lidx;

    ndata = data;

    list_iter_start (rl, &iteridx);
    while ((lidx = list_iter_next (rl, &iteridx)) != MKC_ITER_FINISH) {
      value_t   *valstr;
      value_t   *valval;
      char          str [MKC_VNAME_MAX];
      char          val [MKC_VNAME_MAX];
      char          *tdata = NULL;

      valstr = list_get_by_idx (rl, lidx);
      lidx = list_iter_next (rl, &iteridx);
      if (lidx == MKC_ITER_FINISH) {
        fprintf (stderr, "ERR: replace-list not paired\n");
        mkc_error_set (process->mkcerr, MKC_ERR_FATAL_ERROR, 0, "replace list not paired");
        return NULL;
      }
      valval = list_get_by_idx (rl, lidx);
      sv_value_get_str (process->sv, valstr, str, sizeof (str));
      sv_value_get_str (process->sv, valval, val, sizeof (val));
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
process_alternate_free (void *tchkcontext)
{
  mkc_alternate_t    *alt = tchkcontext;

  if (alt == NULL) {
    return;
  }

  datafree (alt->name);
  list_free (alt->hdrlist);
  list_free (alt->compflags);
  list_free (alt->linkflags);
  list_free (alt->libs);
}

static void
process_list_to_flags (process_t *process, value_t *value,
    list_t *flags, bool inlist)
{
  listidx_t   iteridx;
  listidx_t   lidx;

  list_iter_start (value->list, &iteridx);
  while ((lidx = list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    value_t     * lvalue;
    value_t     rvalue;
    value_t     * tvalue = &rvalue;
    char        flag [MKC_VNAME_MAX];

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = list_get_by_idx (value->list, lidx);
    sv_value_get_value (process->sv, lvalue, tvalue);
    if (mkc_error_chk_err (process->mkcerr)) {
      char    tmp [MKC_VNAME_MAX];

      value_to_str (lvalue, tmp, sizeof (tmp), 0);
      mkc_error_set (process->mkcerr, MKC_ERR_UNKNOWN_VARIABLE, 0, tmp);
      continue;
    }

    if (tvalue->vtype == MKC_VT_LIST) {
      process_list_to_flags (process, tvalue, flags, true);
      if (tvalue->isallocated) {
        value_free (tvalue);
      }
      continue;
    } else if (tvalue->vtype == MKC_VT_STRING ||
        tvalue->vtype == MKC_VT_STATIC_STRING ||
        tvalue->vtype == MKC_VT_QUOTED_STRING) {
      if (! *tvalue->sval) {
        continue;
      }
    } else {
      mkc_error_set (process->mkcerr, MKC_ERR_UNEXPECTED_VALUE_TYPE, 0, NULL);
    }

    sv_value_get_str (process->sv, tvalue, flag, sizeof (flag));
    list_set (flags, tvalue);
    if (tvalue->isallocated) {
      value_free (tvalue);
    }
  }
}

static void
process_save_cache_profile (process_t *process, FILE *fh,
    sv_iter_t *sviter, const char *profname, int *tcount)
{
  scopedvar_t   *sv;
  mkc_varidx_t  viter;
  mkc_varidx_t  vidx;
  int           count = 0;
  sv_type_t     svtype;
  char          *tbuff = NULL;
  char          *tmp = NULL;
  size_t        tmpsz;
  char          *scbuff = NULL;
  char          *p;
  char          *scend;

  sv = process->sv;
  svtype = sv_iter_get_type (sv, sviter);

  if (svtype == SV_T_LOCAL) {
    return;
  }

  tmp = malloc (MKC_SMALL_BUFF_SZ);
  if (tmp == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  *tmp = '\0';
  tmpsz = MKC_SMALL_BUFF_SZ;

  tbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (tmp);
    return;
  }
  *tbuff = '\0';

  scbuff = malloc (MKC_LARGE_BUFF_SZ);
  if (scbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (tmp);
    free (scbuff);
    return;
  }
  *scbuff = '\0';
  p = scbuff;
  scend = scbuff + MKC_LARGE_BUFF_SZ;

  snprintf (tmp, tmpsz, "  profile %s {\n", profname);
  p = stpecpy (p, scend, tmp);
  if (svtype == SV_T_CURR_PROF_COMPILER) {
    mkc_compiler_t    compiler;

    compiler = sv_iter_get_compiler (sv, sviter);
    if (compiler != MKC_COMPILER_GENERAL) {
      snprintf (tmp, tmpsz, "    compiler %s;\n", compiler_get_name (compiler));
      p = stpecpy (p, scend, tmp);
    }
  }

  sv_var_iter_start (sv, sviter, &viter);
  while ((vidx = sv_var_iter_next (sv, sviter, &viter)) != MKC_ITER_FINISH) {
    const char    *nm;
    value_t       *value;
    const char    *vctxtstr = "";

    nm = sv_var_iter_get_name (sv, sviter, vidx);
    value = sv_var_iter_get_value (sv, sviter, vidx);
    value_to_str (value, tbuff, MKC_SMALL_BUFF_SZ, 0);

    if (value_is_string_type (value)) {
      snprintf (tmp, tmpsz, "    set '%s' '%s' ", nm, tbuff);
    } else {
      snprintf (tmp, tmpsz, "    set '%s' %s ", nm, tbuff);
    }
    p = stpecpy (p, scend, tmp);
    vctxtstr = value_ctxt_str (value->vctxt);
    p = stpecpy (p, scend, "{");
    if (value->vtype == MKC_VT_LIST ||
        value->vtype == MKC_VT_DICT) {
      p = stpecpy (p, scend, "\n      ");
    } else {
      p = stpecpy (p, scend, " ");
    }
    snprintf (tmp, tmpsz, "context %s;", vctxtstr);
    p = stpecpy (p, scend, tmp);
    if (value->vtype == MKC_VT_LIST ||
        value->vtype == MKC_VT_DICT) {
      p = stpecpy (p, scend, "\n    }\n");
    } else {
      p = stpecpy (p, scend, " }\n");
    }
    ++count;
    *tcount += 1;
  }

  if (count == 0) {
    p = stpecpy (p, scend, "    ;\n");
  }
  p = stpecpy (p, scend, "  }\n\n");
  if (count > 0) {
    fprintf (fh, scbuff);
  }

  free (scbuff);
  free (tmp);
  free (tbuff);
}


/* checks the .mkc timestamp to see if it has changed */
/* this must be run by any statement process that uses the */
/* dependencies and timestamps, as it is not known if the */
/* project statement exists */
static void
process_check_mkc_timestamp (process_t *process)
{
  int64_t     cachedts = 0;
  bool        changed = false;

  if (process->mkc_ts_checked) {
    return;
  }

  process->mkc_ts_checked = true;

  if (sv_is_defined (process->sv, SV_T_BUILD_DATA,
      process->mkcoptions->mkc_filename, MKC_C_BVAR_TIMESTAMP)) {
    cachedts = sv_get_timestamp (process->sv, SV_T_BUILD_DATA,
        process->mkcoptions->mkc_filename);
  }

  if (process->mkc_ts > cachedts) {
    mkc_message (MKC_V_TMI, "   .mkc ts: changed: true\n");
    changed = true;
    process->mkc_changed = true;
    process->reset_stage = true;
  }

  process_clean_check (process);

  if (changed) {
    sv_set_integer (process->sv, SV_T_INTERNAL,
        MKC_C_MKC_CHANGED, NULL, changed, MKC_VCTXT_MKC);
  }

  sv_set_timestamp (process->sv, SV_T_BUILD_DATA,
      process->mkcoptions->mkc_filename, MKC_C_BVAR_TIMESTAMP,
      process->mkc_ts, MKC_VCTXT_MKC);
}

static void
process_clean_check (process_t *process)
{
  char          tbuff [MKC_PATH_MAX];
  sv_iter_t     * sviter;
  scopedvar_t   * sv = process->sv;
  const char    * profname;
  mkc_varidx_t  viter;
  mkc_varidx_t  vidx;

  if (process->cleaned) {
    return;
  }

  if (! process->reset_stage && ! process->mkcoptions->clean) {
    return;
  }

  mkc_message (MKC_V_BASIC, "-- cleaning\n");
  mkc_clean_mkcfiles (process->projectname, tbuff, MKC_PATH_MAX, process->mkcerr);
  process->cleaned = true;

// ### re-write to use build-data dd
#if 0
  sviter = sv_iter_start (sv, SV_ITER_PROFILES);
  while ((profname = sv_iter_next (sv, sviter)) != NULL) {
    if (sv_iter_get_type (sv, sviter) == SV_T_BUILD) {
      break;
    }
  }

  if (sv_iter_get_type (sv, sviter) != SV_T_BUILD) {
    return;
  }

  sv_var_iter_start (sv, sviter, &viter);
  while ((vidx = sv_var_iter_next (sv, sviter, &viter)) != MKC_ITER_FINISH) {
    value_t     * value;
    const char  * nm;
    int         tgttype;

    nm = sv_var_iter_get_name (sv, sviter, vidx);
    value = sv_var_iter_get_value (sv, sviter, vidx);
    tgttype = sv_value_get_integer (sv, value);

    if (tgttype == TGT_T_INCLUDE ||
        tgttype == TGT_T_SOURCE) {
      continue;
    }

    sv_delete (sv, SV_T_BUILD_DATA, nm, MKC_C_BVAR_DEPENDENCY);
  }
  sv_iter_finish (sviter);
#endif
}

/* debug processing */

static void
process_dbg_print_var (process_t *process, const char *profname)
{
  sv_iter_t         * sviter;
  bool              intest = false;
  char              * tbuff;
  scopedvar_t       * sv;
  const char        * svprofname;
  sv_iter_flag_t    itertype;

  tbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  *tbuff = '\0';

  sv = process->sv;

  itertype = SV_ITER_PROFILES;
  if (profname != NULL) {
    if (strcmp (profname, "test") == 0) {
      profname = MKC_C_PROF_NAME_DEFAULT;
      intest = true;
      itertype = SV_ITER_PROFILES;
    } else if (strcmp (profname, "hierarchy") == 0) {
      profname = NULL;
      itertype = SV_ITER_HIERARCHY;
    } else if (strcmp (profname, "profiles") == 0) {
      profname = NULL;
      itertype = SV_ITER_PROFILES;
    }
  }

  sviter = sv_iter_start (sv, itertype);
  while ((svprofname = sv_iter_next (sv, sviter)) != NULL) {
    mkc_varidx_t    viter;
    mkc_varidx_t    vidx;
    bool            hdr = false;

    if (profname != NULL && strcmp (svprofname, profname) != 0) {
      continue;
    }

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    sv_var_iter_start (sv, sviter, &viter);
    while ((vidx = sv_var_iter_next (sv, sviter, &viter)) != MKC_ITER_FINISH) {
      const char  * nm;
      value_t     * value;

      if (! hdr) {
        sv_type_t         svtype;
        mkc_compiler_t    compiler;
        const char        *compstr = "";

        svtype = sv_iter_get_type (sv, sviter);
        if (svtype == SV_T_CURR_PROF_COMPILER) {
          compiler = sv_iter_get_compiler (sv, sviter);
          compstr = compiler_get_name (compiler);
          fprintf (stdout, "== %s %s\n", svprofname, compstr);
        } else {
          fprintf (stdout, "== %s\n", svprofname);
        }

        hdr = true;
      }

      nm = sv_var_iter_get_name (sv, sviter, vidx);
      value = sv_var_iter_get_value (sv, sviter, vidx);

      if (intest) {
        if (strcmp (nm, "BISON") == 0 ||
            strcmp (nm, "CC") == 0 ||
            strcmp (nm, "CXX") == 0 ||
            strcmp (nm, "DC") == 0 ||
            strcmp (nm, "FLEX") == 0 ||
            strcmp (nm, "OBJC") == 0 ||
            strcmp (nm, "PREFIX") == 0 ||
            strcmp (nm, MKC_C_IVARMACRO) == 0) {
          continue;
        }
      }

      value_to_str (value, tbuff, MKC_SMALL_BUFF_SZ, 0);
      if (value_is_string_type (value)) {
        fprintf (stdout, "  %s '%s'\n", nm, tbuff);
      } else {
        fprintf (stdout, "  %s %s\n", nm, tbuff);
      }
    }
  }
  sv_iter_finish (sviter);

  free (tbuff);
}

static void
process_dbg_print_prof (process_t *process, sv_iter_flag_t sviterflag)
{
  scopedvar_t * sv;
  sv_iter_t   * sviter = NULL;
  const char  * profname;

  sv = process->sv;

  if (sviterflag == SV_ITER_PROFILES) {
    fprintf (stdout, "== profiles\n");
  }
  if (sviterflag == SV_ITER_HIERARCHY) {
    fprintf (stdout, "== hierarchy\n");
  }

  sviter = sv_iter_start (sv, sviterflag);
  while ((profname = sv_iter_next (sv, sviter)) != NULL) {
    sv_type_t  svtype;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    svtype = sv_iter_get_type (sv, sviter);
    if (svtype == SV_T_CURR_PROF_COMPILER) {
      mkc_compiler_t    compiler;

      compiler = sv_iter_get_compiler (sv, sviter);
      fprintf (stdout, "  %s %s %s\n", scopedvar_type_disp (svtype), profname, compiler_get_name (compiler));
    } else {
      fprintf (stdout, "  %s %s\n", scopedvar_type_disp (svtype), profname);
    }
  }
  sv_iter_finish (sviter);
}

static void
process_dbg_print_path (process_t *process)
{
  char    tbuff [MKC_PATH_MAX];

  fprintf (stdout, "== paths\n");
  for (int i = 0; i < MKC_PATH_BUILD_MAX; ++i) {
    path_build (i, tbuff, sizeof (tbuff), NULL, process->mkcerr);
    fprintf (stdout, "  %s %s\n", pathdesc [i], tbuff);
  }
}

static void
process_dbg_print_int_var (process_t *process)
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
  fprintf (stdout, "  changed %d\n", process->mkc_changed);
  fprintf (stdout, "  cleaned %d\n", process->cleaned);
  fprintf (stdout, "  mkc-ts-checked %d\n", process->mkc_ts_checked);
}

static void
process_dbg_print_info (process_t *process)
{
  fprintf (stdout, "== info\n");
  fprintf (stdout, "  int %zd\n", sizeof (int));
  fprintf (stdout, "  time_t %zd\n", sizeof (time_t));
}
