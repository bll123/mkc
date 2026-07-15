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

#include "mkc_asttoken.h"
#include "mkc_check.h"
#include "mkc_const.h"
#include "mkc_context.h"
#include "mkc_dirmatch.h"
#include "mkc_def.h"
#include "mkc_env.h"
#include "mkc_error.h"
#include "mkc_fileop.h"
#include "mkc_log.h"
#include "mkc_option.h"
#include "mkc_path.h"
#include "mkc_process.h"
#include "mkc_profile.h"
#include "mkc_pvar.h"
#include "mkc_regex.h"
#include "mkc_string.h"
#include "mkc_tmutil.h"
#include "mkc_toposort.h"
#include "mkc_var.h"      // for debugging

enum {
  MKC_AUTO_DEFINE_ZERO,
  MKC_AUTO_SKIP_ZERO,
  MKC_INC_READ,
  MKC_INC_NAME_ONLY,
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
  mkc_list_t          *namelist;
  mkc_value_t         *listval;      // list or range
  mkc_value_t         tvalue;
  mkc_profidx_t       plocalidx;
  mkc_listidx_t       iteridx;
} mkc_foreach_t;

typedef struct mkc_process_t {
  mkc_profile_t         * profiles;
  mkc_pvar_t            * pvar;
  mkc_check_t           * check;
  mkc_context_t         * context;
  mkc_error_t           * mkcerr;
  mkc_log_t             * log;
  mkc_option_t          * mkcoptions;
  char                  * projectname;
  const char            * objext;
  const char            * exeext;
  mkc_regex_t           * rxshellvar;
  mkc_regex_t           * rxincguard;
  mkc_list_t            * user_rx_list;
  mkc_attribute_t       attr;
  /* internal */
  mkc_compiler_t        dfltcompiler;
  mkc_system_type_t     systype;
  mkc_system_id_t       sysid;
  mkc_compiler_id_t     compid;
  mkc_lib_loc_t         libloc;
  mkc_header_t          headertype;
  mkc_profidx_t         pidx_curr_comp;
  mkc_profidx_t         pidx_internal;
  bool                  variadicmacro;
  bool                  cacheloaded;
  bool                  cacheinvalidated;
} mkc_process_t;

static const char *sysnames [MKC_SYS_MAX] = {
  [MKC_SYS_AIX] = "MKC_SYS_AIX",
  [MKC_SYS_ANDROID] = "MKC_SYS_ANDROID",
  [MKC_SYS_BSD] = "MKC_SYS_BSD",
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
static char const * const MKC_C_LOOPLIMIT = "MKC_TV_LOOP_LIMIT";
static char const * const MKC_C_TEMPVARPFX = "MKC_TV_";
static size_t mkctvpfxlen = 0;
static char const * const MKC_C_IVARMACRO = "MKC_I_VARIADIC_MACRO";
static char const * const MKC_C_PROJECT_NAME = "MKC_PROJECT_NAME";
static char const * const MKC_C_PROJECT_VERS = "MKC_PROJECT_VERSION";
static char const * const MKC_C_PROJECT_LIB_VERS = "MKC_PROJECT_LIBRARY_VERSION";
static char const * const MKC_C_PATH = "MKC_PATH";
static char ** mkc_process_get_cflags (mkc_process_t *process);

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
static void mkc_process_substitutions (mkc_process_t *process, mkc_value_t *value);
static void mkc_process_alternate_free (void *talt);
static void mkc_process_temp_value_free (void *tvalue);
static void mkc_process_topo_add_items (mkc_process_t *process, mkc_toposort_t *topo, mkc_list_t *hlist);
static void mkc_process_topo_add_deps (mkc_process_t *process, mkc_toposort_t *topo, char *rbuff, const char *hdr);
static mkc_list_t * mkc_process_get_include_list (mkc_process_t *process, mkc_regex_t *rx);
static char * mkc_process_iter_includes (mkc_process_t *process, mkc_list_t *hlist, mkc_listidx_t *hiteridx, char *hdr, size_t hsz, int readflag);
void mkc_process_free_flags (char **flags);


MKC_NODISCARD
mkc_process_t *
mkc_process_init (mkc_profile_t *profiles, mkc_log_t *log,
    mkc_context_t *context, mkc_option_t *mkcoptions, mkc_error_t *mkcerr)
{
  mkc_process_t     *process;
  mkc_profidx_t     pidx;
  int               rc;

  mkctvpfxlen = strlen (MKC_C_TEMPVARPFX);
  process = malloc (sizeof (mkc_process_t));

  process->profiles = profiles;
  process->dfltcompiler = mkc_profile_get_dflt_compiler (profiles);
  process->log = log;
  process->context = context;
  process->mkcoptions = mkcoptions;
  process->check = NULL;
  process->pvar = NULL;
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
  for (int i = 0; i < MKC_ATTR_MAX; ++i) {
    process->attr.str [i] = NULL;
  }
  process->attr.negate = false;
  process->attr.define_zero = MKC_AUTO_SKIP_ZERO;
  process->attr.localheader = false;
  process->attr.printerrors = false;

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

  process->pvar = mkc_pvar_init (process->profiles, log, mkcerr);
  if (process->pvar == NULL) {
    mkc_process_free (process);
    return NULL;
  }

  pidx = mkc_profile_get_active (process->profiles);
  process->pidx_curr_comp = pidx;

  process->pidx_internal = mkc_profile_find (process->profiles,
      MKC_C_PROF_INTERNAL_NAME, MKC_COMPILER_GENERAL);

  process->check = mkc_check_init (process->profiles, process->pvar,
     &process->attr, log, process->pidx_curr_comp, mkcerr);
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

  mkc_pvar_profile_set_idx (process->pvar, pidx);

  return process;
}

void
mkc_process_free (mkc_process_t *process)
{
  if (process == NULL) {
    return;
  }

  if (process->pvar != NULL) {
    mkc_pvar_free (process->pvar);
  }
  if (process->check != NULL) {
    mkc_check_free (process->check);
  }
  datafree (process->projectname);

  mkc_process_attr_clear (process);
  mkc_list_free (process->attr.alternates);
  mkc_list_free (process->attr.pathlist);
  mkc_list_free (process->attr.replacelist);

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
mkc_process_condition (mkc_process_t *process, mkc_value_t *value)
{
  int32_t   rval;

  if (process == NULL) {
    return 0;
  }

  rval = mkc_pvar_value_get_integer (process->pvar, value);
  return rval;
}

void
mkc_process_range_init (mkc_process_t *process,
    mkc_value_t *value, mkc_value_t *beg, mkc_value_t *end, mkc_value_t *incr)
{
  int32_t     ibeg, iend, iincr;

  ibeg = mkc_pvar_value_get_integer (process->pvar, beg);
  iend = mkc_pvar_value_get_integer (process->pvar, end);
  iincr = mkc_pvar_value_get_integer (process->pvar, incr);
  if (mkc_error_chk_err (process->mkcerr)) {
    return;
  }
  mkc_value_range_init (value, ibeg, iend, iincr);
}

int32_t
mkc_process_num_op (mkc_process_t *process, int type,
    mkc_value_t *vala, mkc_value_t *valb)
{
  int32_t   result = 0;
  int32_t   ivala, ivalb;
  char      tbuff [MKC_VNAME_MAX];

  if (process == NULL) {
    return 0;
  }

  mkc_log (process->log, MKC_LOG_PROCESS, "  p-num-op-a: %s\n",
      mkc_value_to_str (vala, tbuff, sizeof (tbuff)));
  mkc_log (process->log, MKC_LOG_PROCESS, "  p-num-op-b: %s\n",
      mkc_value_to_str (valb, tbuff, sizeof (tbuff)));
  ivala = mkc_pvar_value_get_integer (process->pvar, vala);
  ivalb = mkc_pvar_value_get_integer (process->pvar, valb);
  if (mkc_error_chk_err (process->mkcerr)) {
    return 0;
  }
  mkc_log (process->log, MKC_LOG_PROCESS,
      "  p-num-op: %d %d\n", ivala, ivalb);

  switch (type) {
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
mkc_process_str_op (mkc_process_t *process, int type,
    mkc_value_t *vala, mkc_value_t *valb)
{
  int32_t     result = 0;
  char        stra [MKC_PATH_MAX];
  char        strb [MKC_PATH_MAX];

  if (process == NULL) {
    return 0;
  }

  mkc_log (process->log, MKC_LOG_PROCESS, "  p-str-op-a: %s\n",
      mkc_value_to_str (vala, stra, sizeof (stra)));
  mkc_log (process->log, MKC_LOG_PROCESS, "  p-str-op-b: %s\n",
      mkc_value_to_str (valb, strb, sizeof (strb)));
  mkc_pvar_value_get_str (process->pvar, vala, stra, sizeof (stra));
  mkc_pvar_value_get_str (process->pvar, valb, strb, sizeof (strb));
  if (mkc_error_chk_err (process->mkcerr)) {
    return 0;
  }
  mkc_log (process->log, MKC_LOG_PROCESS, "  p-str-op: |%s|%s|\n",
      stra, strb);

  switch (type) {
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
    case MKC_T_OP_STR_EQ_REGEX: {
      mkc_user_regex_t    *urx;

      urx = mkc_process_user_regex_init (process, strb);
      if (urx == NULL) {
        break;
      }
      result = 0;
#if _have_regex
      result = mkc_regex_match (urx->rx, stra);
#endif
      break;
    }
    case MKC_T_OP_STR_NE_REGEX: {
      mkc_user_regex_t    *urx;

      urx = mkc_process_user_regex_init (process, strb);
      if (urx == NULL) {
        break;
      }
      result = 0;
#if _have_regex
      result = ! mkc_regex_match (urx->rx, stra);
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
mkc_process_unary_op (mkc_process_t *process, int type, mkc_value_t *vala)
{
  int32_t     result = 0;
  int32_t     ivala = 0;

  if (process == NULL) {
    return 0;
  }

  if (vala->vtype != MKC_VT_INTEGER) {
    mkc_error_set (process->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT_TYPE, 0, NULL);
    return 0;
  }

  ivala = mkc_pvar_value_get_integer (process->pvar, vala);
  if (mkc_error_chk_err (process->mkcerr)) {
    return 0;
  }

  switch (type) {
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
mkc_process_other_op (mkc_process_t *process, int type, mkc_value_t *vala)
{
  int32_t     result = 0;
  char        *tbuff;

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

  switch (type) {
    case MKC_T_OP_FILE_EXISTS: {
      mkc_pvar_value_get_str (process->pvar, vala, tbuff, MKC_PATH_MAX);
      result = mkc_file_exists (tbuff);
      break;
    }
    case MKC_T_OP_IS_DEFINED: {
      mkc_pvar_value_get_str (process->pvar, vala, tbuff, MKC_PATH_MAX);
      result = mkc_pvar_is_defined (process->pvar, tbuff);
      break;
    }
    case MKC_T_OP_IS_DIRECTORY: {
      mkc_pvar_value_get_str (process->pvar, vala, tbuff, MKC_PATH_MAX);
      result = mkc_is_directory (tbuff);
      break;
    }
    case MKC_T_OP_IS_LIST: {
      mkc_pvar_value_get_str (process->pvar, vala, tbuff, MKC_PATH_MAX);
      result = mkc_pvar_is_list (process->pvar, tbuff);
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
    mkc_value_t *valpath, mkc_value_t *valfn,
    char *buff, size_t sz)
{
  char      *p = buff;
  char      *tbuff;

  *p = '\0';

  if (process == NULL) {
    return;
  }
  if (valfn == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  if (valpath != NULL) {
    mkc_pvar_value_get_str (process->pvar, valpath, tbuff, MKC_PATH_MAX);
    if (strcmp (tbuff, "mkc") == 0) {
    } else {
    }
  }

  mkc_pvar_value_get_str (process->pvar, valfn, tbuff, MKC_PATH_MAX);
  p = stpecpy (p, buff + sz, tbuff);
  free (tbuff);
}

/* control statements */

mkc_foreach_t *
mkc_process_stmt_foreach_setup (mkc_process_t *process,
    mkc_value_t *valnm, mkc_value_t *vallist)
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

  pforeach->plocalidx = mkc_profile_local_create (process->profiles);
  pforeach->namelist = NULL;
  pforeach->listval = NULL;
  pforeach->iteridx = MKC_ITER_FINISH;
  pforeach->tvalue.vtype = MKC_VT_INVALID;

  if (valnm != NULL) {
    mkc_value_t   *value;

    if (valnm->vtype == MKC_VT_RANGE) {
      mkc_error_set (process->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT_TYPE, 0, NULL);
      return NULL;
    }

    value = mkc_pvar_value_get_list_value (process->pvar, valnm);
    pforeach->namelist = value->list;
  }
  if (vallist != NULL) {
    mkc_value_t   *value;

    value = vallist;
    pforeach->listval = mkc_pvar_value_get_list_value (process->pvar, vallist);
    mkc_value_iter_start (value, &pforeach->iteridx);
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
    mkc_value_t     *nval = NULL;
    mkc_listidx_t   rc;

    nval = mkc_list_get_by_idx (pforeach->namelist, nidx);

    rc = mkc_value_iter_next (pforeach->listval, &pforeach->tvalue, &pforeach->iteridx);
    if (rc == MKC_ITER_FINISH) {
      cont = false;
      break;
    }
    mkc_process_local_set (process, nval, &pforeach->tvalue, pforeach->plocalidx);
  }

  return cont;
}

void
mkc_process_stmt_foreach_finish (mkc_process_t *process, mkc_foreach_t *pforeach)
{
  free (pforeach);
  mkc_profile_local_pop (process->profiles);
}

/* statements */

int
mkc_process_stmt_chk_inc_compile (mkc_process_t *process)
{
  int             rc = MKC_ERR_FAILURE;
#if _have_regex
  mkc_list_t      *hlist = NULL;
  mkc_regex_t     *rx;
  mkc_listidx_t   hiteridx;
  char            *hdr;
  char            *rbuff = NULL;
  char            **cflags = NULL;

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

  hdr = malloc (MKC_PATH_MAX);
  if (hdr == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    mkc_process_attr_clear (process);
    return rc;
  }

  rx = mkc_regex_init (process->attr.str [MKC_ATTR_MATCH],
      MKC_REGEX_NONE, process->mkcerr);
  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_process_attr_clear (process);
    free (hdr);
    return rc;
  }

  process->attr.localheader = true;
  process->attr.printerrors = true;
  cflags = mkc_process_get_cflags (process);

  hlist = mkc_process_get_include_list (process, rx);
  mkc_list_iter_start (hlist, &hiteridx);
  while ((rbuff = mkc_process_iter_includes (process, hlist,
      &hiteridx, hdr, MKC_PATH_MAX, MKC_INC_NAME_ONLY)) != NULL) {
    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    rc = mkc_chk_header (process->check, process->attr.currcompiler, hdr,
        (const char **) cflags);
    if (rc != MKC_OK) {
      mkc_error_set (process->mkcerr, MKC_ERR_INCLUDE_COMPILE_FAIL, 0, hdr);
      break;
    }
  }

  mkc_message ("-- check_include_compile - %s\n",
      mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check_include_compile - %s\n",
      mkc_success_msg (rc));

  mkc_process_free_flags (cflags);
  mkc_list_free (hlist);
  mkc_regex_free (rx);
  free (hdr);
#endif
  mkc_process_attr_clear (process);
  return rc;
}

int
mkc_process_stmt_chk_inc_deps (mkc_process_t *process)
{
  mkc_list_t      *hlist = NULL;
  mkc_toposort_t  *topo = NULL;
  mkc_regex_t     *rx = NULL;
  mkc_listidx_t   hiteridx;
  int             rc = MKC_ERR_FAILURE;
  char            *rbuff;
  char            *hdr;

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

  hdr = malloc (MKC_PATH_MAX);
  if (hdr == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    mkc_process_attr_clear (process);
    return rc;
  }

  topo = mkc_toposort_init (process->mkcerr);

#if _have_regex
  rx = mkc_regex_init (process->attr.str [MKC_ATTR_MATCH],
      MKC_REGEX_NONE, process->mkcerr);
#endif
  if (mkc_error_chk_err (process->mkcerr)) {
    free (hdr);
    mkc_process_attr_clear (process);
    return rc;
  }

  hlist = mkc_process_get_include_list (process, rx);
  mkc_process_topo_add_items (process, topo, hlist);

  mkc_list_iter_start (hlist, &hiteridx);
  while ((rbuff = mkc_process_iter_includes (process, hlist, &hiteridx,
      hdr, MKC_PATH_MAX, MKC_INC_READ)) != NULL) {
    mkc_process_topo_add_deps (process, topo, rbuff, hdr);
    free (rbuff);
  }
  mkc_list_free (hlist);

  rc = mkc_toposort (topo);
  if (rc == MKC_ERR_FAILURE) {
    char    tbuff [MKC_VNAME_MAX];

    mkc_toposort_disp_cycle (topo, tbuff, sizeof (tbuff));
    mkc_error_set (process->mkcerr, MKC_ERR_DEPENDENCY_CYCLE, 0, tbuff);
  } else {
    const char  *str;

    mkc_toposort_iter_start (topo);
    while ((str = mkc_toposort_iter_next (topo)) != NULL) {
      mkc_log (process->log, MKC_LOG_PROCESS, " chk-inc-deps: %s\n", str);
    }
  }

  mkc_message ("-- check_include_dependencies - %s\n",
      mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check_include_dependencies - %s\n",
      mkc_success_msg (rc));

#if _have_regex
  mkc_regex_free (rx);
#endif
  mkc_toposort_free (topo);
  free (hdr);
  mkc_process_attr_clear (process);
  return rc;
}

int
mkc_process_stmt_chk_inc_guards (mkc_process_t *process)
{
  int             rc = MKC_ERR_FAILURE;
#if _have_regex
  mkc_list_t      *hlist = NULL;
  mkc_regex_t     *rx;
  mkc_listidx_t   hiteridx;
  char            *rbuff;
  char            *hdr;
  char            **match = NULL;
  int             matchcount;
  mkc_list_t      *guardlist = NULL;

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

  hdr = malloc (MKC_PATH_MAX);
  if (hdr == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    mkc_process_attr_clear (process);
    return rc;
  }

  guardlist = mkc_list_init (MKC_LIST_SORTED, mkc_list_ind_free,
      mkc_list_ind_compare, process->mkcerr);

  if (process->rxincguard == NULL) {
    process->rxincguard = mkc_regex_init (
        "^# *ifndef +([[:alnum:]_][[:alnum:]_]*)[\r\n]+# *define +\\g1$",
        MKC_REGEX_MULTILINE, process->mkcerr);
  }

  if (mkc_error_chk_err (process->mkcerr)) {
    free (hdr);
    mkc_process_attr_clear (process);
    return rc;
  }

  rx = mkc_regex_init (process->attr.str [MKC_ATTR_MATCH],
      MKC_REGEX_NONE, process->mkcerr);
  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_process_attr_clear (process);
    free (hdr);
    return rc;
  }

  rc = MKC_OK;
  hlist = mkc_process_get_include_list (process, rx);
  mkc_list_iter_start (hlist, &hiteridx);
  while ((rbuff = mkc_process_iter_includes (process, hlist, &hiteridx,
      hdr, MKC_PATH_MAX, MKC_INC_READ)) != NULL) {
    char            *tp;
    mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
    mkc_listidx_t   idx;

    if (mkc_error_chk_err (process->mkcerr)) {
      free (rbuff);
      break;
    }

    mkc_regex_get_reset (process->rxincguard);
    match = mkc_regex_get (process->rxincguard, rbuff, &matchcount);
    if (matchcount != 2) {
      mkc_error_set (process->mkcerr, MKC_ERR_INCLUDE_GUARD_NOTFOUND, 0, hdr);
      rc = MKC_ERR_FAILURE;
    }

    if (matchcount == 2) {
      tp = strdup (match [1]);
      loc = MKC_LIST_NOTFOUND;
      idx = mkc_list_find (guardlist, &tp, &loc);
      if (idx != MKC_LIST_NOTFOUND) {
        mkc_error_set (process->mkcerr, MKC_ERR_INCLUDE_GUARD_DUPLICATE, 0, hdr);
        rc = MKC_ERR_FAILURE;
      }

      mkc_list_set (guardlist, &tp, sizeof (char *), &loc);
    }
    mkc_regex_get_free (match);
    free (rbuff);
  }

  mkc_message ("-- check_include_guards - %s\n",
      mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check_include_guards - %s\n",
      mkc_success_msg (rc));

  mkc_list_free (hlist);
  mkc_list_free (guardlist);
  mkc_regex_free (rx);
  free (hdr);
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
    mkc_value_t *value, mkc_value_t *subvalue)
{
  char    tbuff [MKC_VNAME_MAX];

  mkc_pvar_value_get_str (process->pvar, value, tbuff, sizeof (tbuff));

  if (strcmp (tbuff, "null") == 0) {
    /* do nothing */ ;
  }
  if (strcmp (tbuff, "printprof") == 0) {
    mkc_process_dbg_print_prof (process);
  }
  if (strcmp (tbuff, "printvar") == 0) {
    mkc_pvar_value_get_str (process->pvar, subvalue, tbuff, sizeof (tbuff));
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
mkc_process_stmt_function_call (mkc_process_t *process,
    mkc_value_t *valparams, mkc_value_t *valfuncargs)
{
  mkc_profidx_t   plocalidx;
  mkc_list_t      *paramlist = NULL;
  mkc_list_t      *alist = NULL;
  mkc_listidx_t   aiteridx;
  mkc_listidx_t   nmiteridx;
  mkc_listidx_t   aidx;
  mkc_listidx_t   nmidx;

  plocalidx = mkc_profile_local_create (process->profiles);

  if (valparams != NULL) {
    mkc_value_t   *value;

    value = mkc_pvar_value_get_list_value (process->pvar, valparams);
    if (value->vtype == MKC_VT_RANGE) {
      mkc_error_set (process->mkcerr, MKC_ERR_MISMATCHED_ARGUMENT_TYPE, 0, NULL);
      return;
    }
    paramlist = value->list;
  }
  if (valfuncargs != NULL) {
    mkc_value_t   *value = NULL;
    value = mkc_pvar_value_get_list_value (process->pvar, valfuncargs);
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
    mkc_value_t     *aval;
    mkc_value_t     *nmval;

    nmidx = mkc_list_iter_next (paramlist, &nmiteridx);

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    aval = mkc_list_get_by_idx (alist, aidx);
    nmval = mkc_list_get_by_idx (paramlist, nmidx);
    mkc_process_local_set (process, nmval, aval, plocalidx);
  }
}

/* this function is called twice.  once with fromcache == true, */
/* and once with fromcache == false when done */
void
mkc_process_stmt_loadcache (mkc_process_t *process,
    mkc_value_t *valvers, bool fromcache)
{
  int     version;

  version = mkc_pvar_value_get_integer (process->pvar, valvers);
  if (version != 1) {
    mkc_message ("-- cache version mismatch\n");
    mkc_process_attr_clear (process);
    return;
  }

  if (fromcache) {
    process->cacheloaded = true;
  }

  mkc_pvar_set_fromcache (process->pvar, fromcache);

  if (! fromcache && process->cacheloaded && process->cacheinvalidated) {
    mkc_profidx_t   piter;
    mkc_profidx_t   tpidx;

    /* something changed that requires cache invalidation */
    mkc_profile_iter_start (process->profiles, &piter);
    while ((tpidx = mkc_profile_iter_next (process->profiles, &piter)) != MKC_ITER_FINISH) {
      if (mkc_pvar_profile_set_idx (process->pvar, tpidx) == MKC_PROF_NOT_FOUND) {
        continue;
      }
      mkc_profile_clear (process->profiles, tpidx);
    }

    mkc_message ("-- cache invalidated\n");
    mkc_log (process->log, MKC_LOG_GENERAL, "-- cache invalidated\n");
    mkc_process_set_defaults (process);
    mkc_process_int_checks (process);
    mkc_process_get_path (process);
    mkc_process_find_executables (process);
  }

  mkc_process_attr_clear (process);
  return;
}

void
mkc_process_stmt_mark (mkc_process_t *process,
    mkc_value_t *vala, mkc_value_t *valb)
{
  char    nm [MKC_VNAME_MAX];
  char    val [MKC_VNAME_MAX];

  if (process == NULL) {
    return;
  }

  mkc_pvar_value_get_str (process->pvar, vala, nm, sizeof (nm));
  mkc_pvar_value_get_str (process->pvar, valb, val, sizeof (val));
  if (*nm == '\0') {
    mkc_error_set (process->mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
    mkc_process_attr_clear (process);
    return;
  }
  if (strcmp (val, "disable-output") == 0 ||
      strcmp (val, "disable") == 0) {
    mkc_pvar_set_context (process->pvar, nm, MKC_VCTXT_USER_DISABLE);
  } else if (strcmp (val, "enable-output") == 0 ||
      strcmp (val, "enable") == 0) {
    mkc_pvar_set_context (process->pvar, nm, MKC_VCTXT_USER_ENABLE);
  } else {
    mkc_error_set (process->mkcerr, MKC_ERR_PROC_INVALID_MARK, 0, NULL);
  }

  mkc_process_attr_clear (process);
  return;
}

void
mkc_process_stmt_print (mkc_process_t *process, mkc_value_t *value, int depth)
{
  char      tbuff [MKC_PATH_MAX];

  if (process == NULL) {
    return;
  }
  if (value == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  mkc_pvar_value_get_str (process->pvar, value, tbuff, sizeof (tbuff));
  fprintf (stdout, "%s", tbuff);

  if (depth == 0) {
    fprintf (stdout, "\n");
    fflush (stdout);
  }
}

void
mkc_process_stmt_profile (mkc_process_t *process, mkc_value_t *valnm)
{
  char              nm [MKC_VNAME_MAX];
  mkc_profidx_t     pidx;

  mkc_pvar_value_get_str (process->pvar, valnm, nm, sizeof (nm));

  mkc_profile_push (process->profiles);
  /* the attribute for the profile is not yet set. */
  pidx = mkc_profile_find (process->profiles, nm, MKC_COMPILER_GENERAL);

  if (pidx == MKC_PROF_NOT_FOUND) {
    /* has to be a type-current */
    pidx = mkc_profile_create (process->profiles, nm,
        MKC_COMPILER_GENERAL, MKC_PROF_TYPE_CURRENT);
  }

  mkc_pvar_profile_set_idx (process->pvar, pidx);
}

void
mkc_process_stmt_profile_post (mkc_process_t *process)
{
  mkc_profidx_t   pidx;

  pidx = mkc_profile_pop (process->profiles);
  mkc_profile_set_active (process->profiles, pidx);
  process->attr.currcompiler = MKC_COMPILER_GENERAL;
}

void
mkc_process_stmt_project (mkc_process_t *process)
{
  mkc_profidx_t   pidx;
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

  pidx = mkc_profile_get_active (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);
  mkc_pvar_set_str (process->pvar, MKC_C_PROJECT_NAME, process->projectname, MKC_VCTXT_MKC);
  if (process->attr.str [MKC_ATTR_VERSION] != NULL) {
    mkc_pvar_set_str (process->pvar, MKC_C_PROJECT_VERS, process->attr.str [MKC_ATTR_VERSION], MKC_VCTXT_MKC);
  }
  if (process->attr.str [MKC_ATTR_LIB_VERSION] != NULL) {
    mkc_pvar_set_str (process->pvar, MKC_C_PROJECT_LIB_VERS, process->attr.str [MKC_ATTR_LIB_VERSION], MKC_VCTXT_MKC);
  }
  mkc_pvar_profile_set_idx (process->pvar, pidx);

  mkc_process_attr_clear (process);

  return;
}

int
mkc_process_stmt_set (mkc_process_t *process,
    mkc_value_t *valnm, mkc_value_t *value)
{
  char              nm [MKC_VNAME_MAX];
  mkc_value_t       tvalue;
  mkc_err_code_t    trc = MKC_ERR_FAILURE;
  mkc_var_ctxt_t    vctxt = MKC_VCTXT_USER_DISABLE;

  if (process == NULL) {
    return trc;
  }
  if (valnm == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    mkc_process_attr_clear (process);
    return trc;
  }

  mkc_pvar_value_get_str (process->pvar, valnm, nm, sizeof (nm));

  if (*nm == '\0') {
    mkc_error_set (process->mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
    mkc_process_attr_clear (process);
    return trc;
  }

  memcpy (&tvalue, value, sizeof (mkc_value_t));
  mkc_process_substitutions (process, &tvalue);
  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_process_attr_clear (process);
    return trc;
  }

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

  trc = mkc_pvar_set (process->pvar, nm, &tvalue, vctxt);
  if (trc == MKC_OK_CHANGE) {
    process->cacheinvalidated = true;
  }

  mkc_process_temp_value_free (&tvalue);

  mkc_process_attr_clear (process);
  return trc;
}

/* attributes */

void
mkc_process_attribute (mkc_process_t *process, mkc_value_t *valname,
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
    case MKC_T_ATTR_CONTEXT:  { attrtype = MKC_ATTR_VCONTEXT; break; }
    case MKC_T_ATTR_INPUT:    { attrtype = MKC_ATTR_INPUT; break; }
    case MKC_T_ATTR_LIBRARY_VERSION: { attrtype = MKC_ATTR_LIB_VERSION; break; }
    case MKC_T_ATTR_MATCH:    { attrtype = MKC_ATTR_MATCH; break; }
    case MKC_T_ATTR_METHOD:   { attrtype = MKC_ATTR_METHOD; break; }
    case MKC_T_ATTR_OUTPUT:   { attrtype = MKC_ATTR_OUTPUT; break; }
    case MKC_T_ATTR_VERSION:  { attrtype = MKC_ATTR_VERSION; break; }
  }

  switch (iasttype) {
    case MKC_T_ATTR_CONTEXT:
    case MKC_T_ATTR_INPUT:
    case MKC_T_ATTR_LIBRARY_VERSION:
    case MKC_T_ATTR_MATCH:
    case MKC_T_ATTR_METHOD:
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
          MKC_CONTEXT_PROJECT | MKC_CONTEXT_ALTERNATE;
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
    mkc_pvar_value_get_str (process->pvar, valname, nm, sizeof (nm));
  }

  switch (iasttype) {
    case MKC_T_ATTR_CONTEXT:
    case MKC_T_ATTR_INPUT:
    case MKC_T_ATTR_LIBRARY_VERSION:
    case MKC_T_ATTR_MATCH:
    case MKC_T_ATTR_METHOD:
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
mkc_process_attr_compiler (mkc_process_t *process, mkc_value_t *name)
{
  char            nm [MKC_VNAME_MAX];
  const char      *profnm;
  mkc_profidx_t   pidx;

  if (process == NULL) {
    return;
  }

  /* the compiler attribute is only allowed in */
  /* project and profile statements */
  if (! mkc_context_check (process->context,
      MKC_CONTEXT_PROJECT | MKC_CONTEXT_PROFILE | MKC_CONTEXT_CHK_INC)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  mkc_pvar_value_get_str (process->pvar, name, nm, sizeof (nm));
  if (mkc_context_check (process->context, MKC_CONTEXT_PROJECT)) {
    /* if in a project statement, the default compiler is set */
    process->dfltcompiler = mkc_compiler_get (nm);
    mkc_profile_set_dflt_compiler (process->profiles, process->dfltcompiler);
  }

  process->attr.currcompiler = mkc_compiler_get (nm);

  pidx = mkc_profile_get_active (process->profiles);
  profnm = mkc_profile_get_name (process->profiles, pidx);

  if (strcmp (profnm, MKC_C_PROF_INTERNAL_NAME) == 0) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  pidx = mkc_profile_find (process->profiles, profnm, process->attr.currcompiler);
  if (pidx == MKC_PROF_NOT_FOUND) {
    pidx = mkc_profile_create (process->profiles, profnm,
        process->attr.currcompiler, MKC_PROF_TYPE_CURRENT);
  }

  if (mkc_context_check (process->context, MKC_CONTEXT_PROFILE)) {
    mkc_profile_set_active (process->profiles, pidx);
  }
}

void
mkc_process_attr_comp_flags (mkc_process_t *process, mkc_value_t *value)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;
  mkc_list_t      * clist;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context,
      MKC_CONTEXT_CHECK | MKC_CONTEXT_ALTERNATE | MKC_CONTEXT_CHK_INC)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  clist = process->attr.curralt->compflags;

  mkc_list_iter_start (value->list, &iteridx);
  while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    mkc_value_t     *lvalue;
    mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    mkc_process_substitutions (process, lvalue);
    if (! *(lvalue->sval)) {
      continue;
    }
    mkc_list_set (clist, lvalue, sizeof (mkc_value_t), &loc);
  }

  return;
}

void
mkc_process_attr_header (mkc_process_t *process, mkc_value_t *value)
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
    mkc_value_t     *lvalue;
    mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    mkc_list_set (hlist, lvalue, sizeof (mkc_value_t), &loc);
  }

  return;
}

void
mkc_process_attr_link_flags (mkc_process_t *process, mkc_value_t *value)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;
  mkc_list_t      * llist;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context,
      MKC_CONTEXT_CHECK | MKC_CONTEXT_ALTERNATE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  llist = process->attr.curralt->linkflags;

  mkc_list_iter_start (value->list, &iteridx);
  while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    mkc_value_t     *lvalue;
    mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    mkc_process_substitutions (process, lvalue);
    if (! *(lvalue->sval)) {
      continue;
    }
    mkc_list_set (llist, lvalue, sizeof (mkc_value_t), &loc);
  }

  return;
}

void
mkc_process_attr_match (mkc_process_t *process, mkc_value_t *str)
{
  return;
}

void
mkc_process_attr_path (mkc_process_t *process, mkc_value_t *path)
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

  mkc_list_set (process->attr.pathlist, path, sizeof (mkc_value_t), &loc);
  return;
}

void
mkc_process_attr_replace (mkc_process_t *process,
    mkc_value_t *str, mkc_value_t *name)
{
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context, MKC_CONTEXT_CONFIGURE)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  mkc_list_set (process->attr.replacelist, str, sizeof (mkc_value_t), &loc);
  mkc_list_set (process->attr.replacelist, name, sizeof (mkc_value_t), &loc);
  return;
}

bool
mkc_process_profile_is_current (mkc_process_t *process, mkc_value_t *valnm)
{
  char              nm [MKC_VNAME_MAX];
  bool              rc = false;

  if (process == NULL) {
    return false;
  }

  mkc_pvar_value_get_str (process->pvar, valnm, nm, sizeof (nm));

  rc = mkc_profile_is_current (process->profiles, nm);
  return rc;
}

int32_t
mkc_process_check (mkc_process_t *process, mkc_value_t *valconst,
    mkc_astnode_token_t asttype)
{
  int         rc = MKC_OK;
  char        tnm [MKC_VNAME_MAX];
  char        txt [MKC_VNAME_MAX];
  char        pfx [MKC_VNAME_MAX];
  mkc_pvar_t  *pvar;
  int         iasttype = asttype;
  bool        successtype = false;
  bool        valtype = false;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  pvar = process->pvar;
  mkc_pvar_value_get_str (process->pvar, valconst, txt, sizeof (txt));
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

    mkc_pvar_set_integer (pvar, tnm, rc == 0 ? true : false, MKC_VCTXT_CHECK);
    mkc_message ("-- check %s: %s : %s - %s\n",
        typenames [asttype], txt, tnm, mkc_success_msg (rc));
    mkc_log (process->log, MKC_LOG_CHECK, "-- check %s: %s : %s - %s\n",
        typenames [asttype], txt, tnm, mkc_success_msg (rc));
    /* set the return code for ast */
    rc = rc == 0 ? MKC_OK : MKC_ERR_FAILURE;
  }
  if (valtype) {
    /* the check is run, and the return code is a value */
    mkc_pvar_set_integer (pvar, tnm, rc, MKC_VCTXT_CHECK);
    mkc_message ("-- check %s: %s : %s : %d\n", typenames [asttype], txt, tnm, rc);
    mkc_log (process->log, MKC_LOG_CHECK,
        "-- check %s: %s : %s : %d\n", typenames [asttype], txt, tnm, rc);
    /* set the return code for ast */
    rc = rc > 0 ? MKC_OK : MKC_ERR_FAILURE;
  }

  mkc_process_attr_clear (process);
  /* a boolean value indicating success should be returned */
  return rc;
}

int32_t
mkc_process_check_flag (mkc_process_t *process,
    mkc_value_t *valflag, int addchk, mkc_astnode_token_t asttype)
{
  int         rc = MKC_OK;
  char        tnm [MKC_VNAME_MAX];
  char        flag [MKC_VNAME_MAX];
  const char  *pfx = NULL;
  mkc_pvar_t  *pvar;
  int         iasttype = asttype;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  mkc_pvar_value_get_str (process->pvar, valflag, flag, sizeof (flag));

  if (! *flag) {
    /* empty flags are ignored */
    return MKC_ERR_FAILURE;
  }

  switch (iasttype) {
    case MKC_T_CHK_COMP_FLAG: { pfx = "cf_"; break; }
    case MKC_T_CHK_LINK_FLAG: { pfx = "lf_"; break; }
  }
  mkc_process_create_name (process, asttype, tnm, sizeof (tnm), pfx, flag, NULL);

  pvar = process->pvar;

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
    mkc_pvar_set_str (pvar, tnm, flag, MKC_VCTXT_FLAG);

    switch (iasttype) {
      case MKC_T_CHK_COMP_FLAG: {
        mkc_pvar_append_str_list (process->pvar, MKC_C_CFLAGS, flag, MKC_VCTXT_MKC);
        break;
      }
      case MKC_T_CHK_LINK_FLAG: {
        const char    *nm = MKC_C_LDFLAGS;

        if (strncmp (flag, "-L", 2) == 0 ||
            strncmp (flag, "-l", 2) == 0) {
          nm = MKC_C_LIBS;
        }
        mkc_pvar_append_str_list (process->pvar, nm, flag, MKC_VCTXT_MKC);
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
    mkc_value_t *valstructnm, mkc_value_t *valmembernm)
{
  int         rc = MKC_OK;
  char        tnm [MKC_VNAME_MAX];
  char        structname [MKC_VNAME_MAX];
  char        membername [MKC_VNAME_MAX];
  mkc_pvar_t  *pvar;
  char        tmpdisp [MKC_VNAME_MAX * 2];

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  pvar = process->pvar;
  mkc_pvar_value_get_str (process->pvar, valstructnm, structname, sizeof (structname));
  mkc_pvar_value_get_str (process->pvar, valmembernm, membername, sizeof (membername));
  mkc_process_create_name (process, MKC_T_CHK_STRUCT_MEMBER, tnm, sizeof (tnm),
      "_member_", structname, membername, NULL);

  snprintf (tmpdisp, sizeof (tmpdisp), "%s.%s", structname, membername);
  if (mkc_process_chk_cache (process, tmpdisp, tnm)) {
    mkc_process_attr_clear (process);
    return rc;
  }

  rc = mkc_chk_struct_member (process->check,
      process->attr.currcompiler, structname, membername);
  mkc_pvar_set_integer (pvar, tnm, rc == 0 ? true : false, MKC_VCTXT_CHECK);

  mkc_message ("-- check struct member: %s.%s - %s\n",
      structname, membername, mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check struct member: %s.%s - %s\n",
      structname, membername, mkc_success_msg (rc));

  mkc_process_attr_clear (process);
  return rc;
}

int
mkc_process_chk_shell_extract (mkc_process_t *process, mkc_value_t *valpath)
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

  mkc_pvar_value_get_str (process->pvar, valpath, path, MKC_PATH_MAX);

  if (! mkc_file_exists (path)) {
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

  buff = mkc_read_file (path, &fsz, process->mkcerr);
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
    tvalue = mkc_pvar_substitute (process->pvar, varvalue, MKC_PV_SUB_ESCAPE, 0);
    if (tvalue == NULL) {
      continue;
    }

    mkc_pvar_set_str (process->pvar, varname, tvalue, MKC_VCTXT_CHECK);

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
mkc_process_local_set (mkc_process_t *process, mkc_value_t *nmval,
    mkc_value_t *argval, mkc_profidx_t pidx)
{
  mkc_profidx_t   opidx;
  char            nm [MKC_VNAME_MAX];

  if (process == NULL) {
    return;
  }
  if (nmval == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  mkc_pvar_value_get_str (process->pvar, nmval, nm, sizeof (nm));

  opidx = mkc_profile_get_active (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, pidx);
  mkc_pvar_set (process->pvar, nm, argval, MKC_VCTXT_TEMP);
  mkc_pvar_profile_set_idx (process->pvar, opidx);
}

int32_t
mkc_process_get_loop_limit (mkc_process_t *process)
{
  int32_t     limit = 10000;
  mkc_value_t *value;

  if (process == NULL) {
    return limit;
  }

  value = mkc_pvar_get_by_profile (process->pvar, MKC_C_LOOPLIMIT);
  if (value != NULL) {
    limit = mkc_pvar_value_get_integer (process->pvar, value);
  }

  return limit;
}

void
mkc_process_save_cache (mkc_process_t *process)
{
  mkc_profidx_t   piter;
  mkc_profile_t   *profiles;
  mkc_profidx_t   opidx;
  mkc_profidx_t   pidx;
  char            *cachename;
  FILE            *fh;
  int             tcount = 0;
  const char      * indent = "  ";
  char            * tbuff;

  if (mkc_error_chk_err (process->mkcerr)) {
    /* at this time, the cache is not saved if there was an error */
    return;
  }

  cachename = malloc (MKC_PATH_MAX);
  if (cachename == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  mkc_path_build (MKC_PATH_MKC_FILES, cachename, MKC_PATH_MAX,
      "cache.mkc", process->mkcerr);
  fh = mkc_fopen (cachename, "w");
  if (fh == NULL) {
    free (cachename);
    return;
  }
  opidx = mkc_profile_get_active (process->profiles);

  tbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  /* version 1 */
  fprintf (fh, "load_cache %d {\n", MKC_CACHE_VERS_1);

  profiles = process->profiles;
  mkc_profile_iter_start (profiles, &piter);
  while ((pidx = mkc_profile_iter_next (profiles, &piter)) != MKC_ITER_FINISH) {
    const char      *nm;
    const char      *compname;
    mkc_varidx_t    viter;
    mkc_varidx_t    vidx;
    mkc_pvar_t      *pvar;
    int             count = 0;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    pvar = process->pvar;
    if (mkc_pvar_profile_set_idx (process->pvar, pidx) == MKC_PROF_NOT_FOUND) {
      continue;
    }

    if (mkc_pvar_size (process->pvar) == 0) {
      continue;
    }

    if (pidx == opidx) {
      indent = "";
    } else {
      nm = mkc_profile_get_name (profiles, pidx);
      compname = mkc_profile_get_comp_name (profiles, pidx);
      fprintf (fh, "  profile %s {\n", nm);
      if (strcmp (nm, MKC_C_PROF_INTERNAL_NAME) != 0) {
        fprintf (fh, "    compiler %s;\n", compname);
      }
      indent = "  ";
    }

    mkc_pvar_iter_start (pvar, &viter);
    while ((vidx = mkc_pvar_iter_next (pvar, &viter)) != MKC_ITER_FINISH) {
      const char    *nm;
      mkc_value_t   *value;
      const char    *vctxtstr = "";

      nm = mkc_pvar_get_name (pvar, vidx);

      /* temporary variables do not need to be cached */
      if (strncmp (nm, MKC_C_TEMPVARPFX, mkctvpfxlen) == 0) {
        continue;
      }

      value = mkc_pvar_get_by_idx (pvar, vidx);
      mkc_value_to_str (value, tbuff, MKC_SMALL_BUFF_SZ);
      if (value->vtype == MKC_VT_INTEGER ||
          value->vtype == MKC_VT_LIST) {
        fprintf (fh, "  %sset %s %s ", indent, nm, tbuff);
      } else {
        fprintf (fh, "  %sset %s '%s' ", indent, nm, tbuff);
      }
      vctxtstr = mkc_var_vctxt_str (value->vctxt);
      fprintf (fh, "{ context %s; }\n", vctxtstr);
      ++count;
      ++tcount;
    }

    if (pidx != opidx) {
      if (count == 0) {
        fprintf (fh, "    ;\n");
      }
      fprintf (fh, "  }\n\n");
    }
    fprintf (fh, "\n");
  }

  if (tcount == 0) {
    fprintf (fh, "  ;\n");
  }
  fprintf (fh, "}\n");

  mkc_pvar_profile_set_idx (process->pvar, opidx);
  free (cachename);
  free (tbuff);
  fclose (fh);
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

  mkc_strclean (buff, nlen);

  return buff;
}

static int
mkc_process_int_checks (mkc_process_t *process)
{
  int                 rc;
  mkc_profidx_t       opidx;
  mstime_t            starttm;
  int                 isystype;

  mstimestart (&starttm);
  mkc_create_dirs ();

  opidx = mkc_profile_get_active (process->profiles);
  mkc_log (process->log, MKC_LOG_CHECK, "== internal checks\n");

  /* environment variables : default/comp */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_curr_comp);
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

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  rc = mkc_chk_compiler_id (process->check, process->dfltcompiler);
  if (rc >= 0) {
    process->compid = rc;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "compiler-id", process->compid);

  for (mkc_compiler_id_t i = 0; i < MKC_COMP_ID_MAX; ++i) {
    if (process->compid == i) {
      mkc_pvar_set_integer (process->pvar, compidnames [i], true, MKC_VCTXT_MKC);
      break;
    }
  }

  mkc_process_attr_clear (process);

  /* modern header support : dflt/comp */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_curr_comp);

  rc = mkc_chk_header_modern (process->check, process->dfltcompiler);
  if (rc != 0) {
    process->headertype = MKC_HEADER_LEGACY;
  }
  process->attr.headertype = process->headertype;
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "header-type", process->headertype);

  mkc_process_attr_clear (process);

  /* system type : internal */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  rc = mkc_chk_system_type (process->check, process->dfltcompiler);
  if (rc >= 0) {
    process->systype = rc;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "system-type", process->systype);

  for (mkc_system_type_t i = 0; i < MKC_SYS_MAX; ++i) {
    if (process->systype == i) {
      mkc_pvar_set_integer (process->pvar, sysnames [i], true, MKC_VCTXT_MKC);
      break;
    }
  }

  mkc_process_attr_clear (process);

  /* object, executable extension : internal */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  if (process->systype == MKC_SYS_WINDOWS) {
    process->objext = ".obj";
    process->exeext = ".exe";
  }
  mkc_pvar_set_str (process->pvar, MKC_C_OBJEXT, process->objext, MKC_VCTXT_MKC);
  mkc_pvar_set_str (process->pvar, MKC_C_EXEEXT, process->exeext, MKC_VCTXT_MKC);

  /* shared library extension : internal */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  /* default is .so */
  mkc_pvar_set_str (process->pvar, MKC_C_SHLIBEXT, ".so", MKC_VCTXT_MKC);
  isystype = process->systype;
  switch (isystype) {
    case MKC_SYS_AIX: {
      mkc_pvar_set_str (process->pvar, MKC_C_SHLIBEXT, ".a", MKC_VCTXT_MKC);
      break;
    }
    case MKC_SYS_MACOS: {
      mkc_pvar_set_str (process->pvar, MKC_C_SHLIBEXT, ".dylib", MKC_VCTXT_MKC);
      break;
    }
    case MKC_SYS_WINDOWS: {
      mkc_pvar_set_str (process->pvar, MKC_C_SHLIBEXT, ".dll", MKC_VCTXT_MKC);
      break;
    }
  }

  /* system id : internal */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  rc = mkc_chk_system_id (process->check, process->dfltcompiler);
  if (rc >= 0) {
    process->sysid = rc;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "system-id", process->sysid);

  for (mkc_system_id_t i = 0; i < MKC_SYS_ID_MAX; ++i) {
    if (process->sysid == i) {
      mkc_pvar_set_integer (process->pvar, sysidnames [i], true, MKC_VCTXT_MKC);
      break;
    }
  }

  mkc_process_attr_clear (process);

  /* variadic macro support : dflt/comp */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_curr_comp);

  rc = mkc_chk_variadic_macro (process->check, process->dfltcompiler);
  if (rc != 0) {
    process->variadicmacro = MKC_NO_VARIADIC_MACRO;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", MKC_C_IVARMACRO, process->variadicmacro);
  mkc_pvar_set_integer (process->pvar, MKC_C_IVARMACRO, process->variadicmacro, MKC_VCTXT_MKC);

  mkc_process_attr_clear (process);

  /* linux: library location : internal */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  if (process->systype == MKC_SYS_LINUX) {
    rc = mkc_chk_library_location (process->check, process->dfltcompiler);
    if (rc >= 0) {
      process->libloc = rc;
    }
    mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", MKC_C_LIBLOCNAME, process->libloc);
    mkc_pvar_set_integer (process->pvar, MKC_C_LIBLOCNAME, process->libloc, MKC_VCTXT_MKC);
  }

  mkc_process_attr_clear (process);

  /* reset profile */

  mkc_pvar_profile_set_idx (process->pvar, opidx);

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

  return MKC_OK;
}

static void
mkc_process_set_defaults (mkc_process_t *process)
{
  /* create internal constants */
  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  for (mkc_system_type_t i = 0; i < MKC_SYS_MAX; ++i) {
    mkc_pvar_set_integer (process->pvar, sysnames [i], false, MKC_VCTXT_MKC);
  }
  for (mkc_system_id_t i = 0; i < MKC_SYS_ID_MAX; ++i) {
    mkc_pvar_set_integer (process->pvar, sysidnames [i], false, MKC_VCTXT_MKC);
  }

  mkc_pvar_set_integer (process->pvar, MKC_C_LOOPLIMIT, 10000, MKC_VCTXT_MKC);
  mkc_pvar_set_integer (process->pvar, MKC_C_LIBLOCNAME, process->libloc, MKC_VCTXT_MKC);

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  mkc_pvar_set_str (process->pvar, "BISON", "bison", MKC_VCTXT_ENV);
  mkc_pvar_set_str (process->pvar, "CC", "cc", MKC_VCTXT_ENV);
  mkc_pvar_set_str (process->pvar, "CXX", "c++", MKC_VCTXT_ENV);
  mkc_pvar_set_str (process->pvar, "FLEX", "flex", MKC_VCTXT_ENV);
  mkc_pvar_set_str (process->pvar, "OBJC", "cc", MKC_VCTXT_ENV);

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);
  for (mkc_compiler_id_t i = 0; i < MKC_COMP_ID_MAX; ++i) {
    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }
    mkc_pvar_set_integer (process->pvar, compidnames [i], false, MKC_VCTXT_MKC);
  }

  mkc_pvar_set_str (process->pvar, MKC_C_PROFILE_NAME,
      mkc_profile_get_name (process->profiles, process->pidx_curr_comp),
      MKC_VCTXT_MKC);

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_curr_comp);
}

static void
mkc_process_configure_manual (mkc_process_t *process)
{
  char    *data;
  char    *ndata;
  size_t  fsz;
  FILE    *fh;

  data = mkc_read_file (process->attr.str [MKC_ATTR_INPUT], &fsz, process->mkcerr);
  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_error_set (process->mkcerr, MKC_ERR_FILE_NOT_FOUND,
        errno, process->attr.str [MKC_ATTR_INPUT]);
    return;
  }
  ndata = mkc_process_configure_substitute (process, data);
  free (data);
  fh = mkc_fopen (process->attr.str [MKC_ATTR_OUTPUT], "w");
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
  mkc_profidx_t   piter;
  mkc_profile_t   *profiles;
  mkc_profidx_t   opidx;
  mkc_profidx_t   pidx;
  FILE            *fh;
  char            *fname;
  char            *tbuff;
  char            *tp;
  char            projnm [MKC_VNAME_MAX];
  size_t          len;

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

  fh = mkc_fopen (fname, "w");
  if (fh == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, fname);
    return;
  }

  opidx = mkc_profile_get_active (process->profiles);

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

  profiles = process->profiles;
  mkc_profile_iter_start (profiles, &piter);
  while ((pidx = mkc_profile_iter_next (profiles, &piter)) != MKC_ITER_FINISH) {
    mkc_varidx_t    viter;
    mkc_varidx_t    vidx;
    mkc_pvar_t      *pvar;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    pvar = process->pvar;
    if (mkc_pvar_profile_set_idx (process->pvar, pidx) == MKC_PROF_NOT_FOUND) {
      continue;
    }

    if (mkc_pvar_size (process->pvar) == 0) {
      continue;
    }

    mkc_pvar_iter_start (pvar, &viter);
    while ((vidx = mkc_pvar_iter_next (pvar, &viter)) != MKC_ITER_FINISH) {
      const char      *nm;
      mkc_value_t     *value;

      nm = mkc_pvar_get_name (pvar, vidx);
      value = mkc_pvar_get_by_idx (pvar, vidx);
      if (value->vctxt != MKC_VCTXT_CHECK &&
          value->vctxt != MKC_VCTXT_USER_ENABLE) {
        continue;
      }

      if (value->vtype == MKC_VT_INTEGER) {
        int32_t     ival;

        ival = value->ival;
        if (defzero == MKC_AUTO_DEFINE_ZERO || ival != 0) {
          fprintf (fh, "#define %s %d\n", nm, ival);
        }
      } else {
        mkc_value_to_str (value, tbuff, MKC_PATH_MAX);
        fprintf (fh, "#define %s \"%s\"\n", nm, tbuff);
      }
    }
  }

  fprintf (fh, "\n");
  fprintf (fh, "#endif /* INC_%s_H */\n", projnm);

  mkc_pvar_profile_set_idx (process->pvar, opidx);
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
  if (mkc_pvar_is_defined (process->pvar, nm)) {
    if (process->mkcoptions->retest) {
      mkc_value_t   *value;

      value = mkc_pvar_get_by_profile (process->pvar, nm);
      if (value->vtype == MKC_VT_INTEGER) {
        int32_t   val;

        val = mkc_pvar_value_get_integer (process->pvar, value);
        if (! val) {
          return rc;
        }
      }
    }

    mkc_message ("-- cached: %s : %s\n", disp, nm);
    rc = true;
  }

  return rc;
}

static void
mkc_process_get_path (mkc_process_t *process)
{
  mkc_profidx_t   opidx;
  char            *tbuff;
  char            *tpath;
  char            *tokstr;
  const char      *pathdelim = ":";


  opidx = mkc_profile_get_active (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  if (process->systype == MKC_SYS_WINDOWS) {
    pathdelim = ";";
  }

  tbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  mkc_env_get ("PATH", tbuff, MKC_SMALL_BUFF_SZ);

  tpath = mkc_strtok (tbuff, pathdelim, &tokstr);
  while (tpath != NULL) {
    mkc_normalize_path (tpath, strlen (tpath));
    mkc_pvar_append_str_list (process->pvar, MKC_C_PATH,
        tpath, MKC_VCTXT_MKC);
    tpath = mkc_strtok (NULL, pathdelim, &tokstr);
  }

  free (tbuff);

  mkc_pvar_profile_set_idx (process->pvar, opidx);
}

static void
mkc_process_find_executables (mkc_process_t *process)
{
  mkc_profidx_t   opidx;
  char            *testpath;
  char            *tpath;
  mkc_prog_chk_t  *chk;
  char            *p;
  mkc_list_t      *pathlist;
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;
  mkc_value_t     *valpath;


  opidx = mkc_profile_get_active (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  testpath = malloc (MKC_PATH_MAX);
  if (testpath == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  valpath = mkc_pvar_get_by_profidx (process->pvar, MKC_C_PATH, process->pidx_internal);
  pathlist = valpath->list;

  chk = proglist;
  while (chk->program != NULL) {
    mkc_list_iter_start (pathlist, &iteridx);
    while ((lidx = mkc_list_iter_next (pathlist, &iteridx)) != MKC_ITER_FINISH) {
      mkc_value_t   *lvalue;

      lvalue = mkc_list_get_by_idx (pathlist, lidx);
      tpath = lvalue->sval;

      p = stpecpy (testpath, testpath + MKC_PATH_MAX, tpath);
      p = stpecpy (p, testpath + MKC_PATH_MAX, "/");
      p = stpecpy (p, testpath + MKC_PATH_MAX, chk->program);
      p = stpecpy (p, testpath + MKC_PATH_MAX, process->exeext);

      if (mkc_file_exists (testpath)) {
        mkc_pvar_set_str (process->pvar, chk->mkcvarname, testpath, MKC_VCTXT_MKC);
        break;
      }
    }
    chk += 1;
  }

  free (testpath);

  mkc_pvar_profile_set_idx (process->pvar, opidx);
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
mkc_process_dbg_print_var (mkc_process_t *process, const char *pname)
{
  mkc_varidx_t    iteridx;
  mkc_varidx_t    vidx;
  mkc_pvar_t      *pvar;
  bool            intest = false;
  bool            ininternal = false;
  mkc_profidx_t   opidx;
  char            *tbuff;

  tbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  pvar = process->pvar;
  opidx = mkc_profile_get_active (process->profiles);

  if (pname != NULL && strcmp (pname, "default") == 0) {
    pname = mkc_profile_get_name (process->profiles, opidx);
  } else if (pname != NULL && strcmp (pname, "test") == 0) {
    intest = true;
    pname = mkc_profile_get_name (process->profiles, opidx);
  } else if (pname != NULL && strcmp (pname, "internalall") == 0) {
    pname = MKC_C_PROF_INTERNAL_NAME;
  } else if (pname != NULL && strcmp (pname, "internal") == 0) {
    ininternal = true;
  }

  for (mkc_compiler_t i = 0; i < MKC_COMPILER_MAX; ++i) {
    bool      hdr = false;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    if (mkc_pvar_profile_set (process->pvar, pname, i) == MKC_PROF_NOT_FOUND) {
      continue;
    }

    mkc_pvar_iter_start (pvar, &iteridx);

    while ((vidx = mkc_pvar_iter_next (pvar, &iteridx)) != MKC_ITER_FINISH) {
      const char      *nm;
      mkc_value_t     *value;

      if (! hdr) {
        fprintf (stdout, "== %s %s\n", pname, mkc_compiler_get_name (i));
        hdr = true;
      }

      nm = mkc_pvar_get_name (pvar, vidx);
      value = mkc_pvar_get_by_idx (pvar, vidx);

      if (ininternal) {
        /* the variables that are used internally are */
        /* generally not interesting */
        if (strncmp (nm, MKC_C_TEMPVARPFX, mkctvpfxlen) == 0) {
          continue;
        }
      }

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

      mkc_value_to_str (value, tbuff, MKC_SMALL_BUFF_SZ);
      if (value->vtype == MKC_VT_INTEGER ||
          value->vtype == MKC_VT_LIST) {
        fprintf (stdout, "  %s %s\n", nm, tbuff);
      } else {
        fprintf (stdout, "  %s '%s'\n", nm, tbuff);
      }
    }
  }

  mkc_pvar_profile_set_idx (process->pvar, opidx);
  free (tbuff);
}

static void
mkc_process_dbg_print_prof (mkc_process_t *process)
{
  mkc_varidx_t   iteridx;
  mkc_varidx_t   idx;
  mkc_profile_t   *profiles;

  fprintf (stdout, "== profiles\n");

  profiles = process->profiles;
  mkc_profile_iter_start (profiles, &iteridx);
  while ((idx = mkc_profile_iter_next (profiles, &iteridx)) != MKC_ITER_FINISH) {
    const char      *nm;
    mkc_compiler_t compiler;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    nm = mkc_profile_get_name (profiles, idx);
    compiler = mkc_profile_get_compiler (profiles, idx);
    fprintf (stdout, "  %s %s\n", nm, mkc_compiler_get_name (compiler));
  }
}

static void
mkc_process_dbg_print_path (mkc_process_t *process)
{
  char    tbuff [MKC_PATH_MAX];

  fprintf (stdout, "== paths\n");
  for (int i = 0; i < MKC_PATH_BUILD_MAX; ++i) {
    mkc_path_build (i, tbuff, sizeof (tbuff), NULL, process->mkcerr);
    fprintf (stdout, "  %s %s\n", pathdesc [i], tbuff);
  }
}

static void
mkc_process_dbg_print_int_var (mkc_process_t *process)
{
  fprintf (stdout, "== internal variables\n");
  fprintf (stdout, "  project-name: %s\n", process->projectname);
  fprintf (stdout, "  default-compiler %d/%s\n", process->dfltcompiler, mkc_compiler_get_name (process->dfltcompiler));
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
    ndata = mkc_pvar_substitute (process->pvar, data, MKC_PV_NO_ESCAPE, 0);
  } else {
    mkc_listidx_t   iteridx;
    mkc_listidx_t   lidx;

    ndata = data;

    mkc_list_iter_start (rl, &iteridx);
    while ((lidx = mkc_list_iter_next (rl, &iteridx)) != MKC_ITER_FINISH) {
      mkc_value_t   *valstr;
      mkc_value_t   *valval;
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
      mkc_pvar_value_get_str (process->pvar, valstr, str, sizeof (str));
      mkc_pvar_value_get_str (process->pvar, valval, val, sizeof (val));
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
mkc_process_substitutions (mkc_process_t *process, mkc_value_t *value)
{
  if (value->vtype == MKC_VT_ENV_VARIABLE ||
      value->vtype == MKC_VT_QUOTED_STRING) {
    char    *buff;

    buff = malloc (MKC_PATH_MAX);
    if (buff == NULL) {
      mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return;
    }

    /* need to get the actual value */
    mkc_pvar_value_get_str (process->pvar, value, buff, MKC_PATH_MAX);
    value->sval = buff;
  }
  if (value->vtype == MKC_VT_VARIABLE) {
    mkc_value_t   *nvalue;

    nvalue = mkc_pvar_get_variable_value (process->pvar, value->sval);
    value->vtype = MKC_VT_INVALID;
    if (nvalue != NULL) {
      memcpy (value, nvalue, sizeof (mkc_value_t));
    }
  }
  if (value->vtype == MKC_VT_LIST) {
    mkc_listidx_t     iteridx;
    mkc_listidx_t     lidx;
    mkc_list_t        *nlist;

    /* each value in a list must be processed, as the value in the list */
    /* may be a variable */

    nlist = mkc_list_init (MKC_LIST_UNSORTED, mkc_process_temp_value_free, NULL, process->mkcerr);
    mkc_list_iter_start (value->list, &iteridx);
    while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
      mkc_value_t   *lvalue;
      mkc_value_t   tvalue;
      mkc_listidx_t loc = MKC_LIST_NOTFOUND;

      if (mkc_error_chk_err (process->mkcerr)) {
        break;
      }
      lvalue = mkc_list_get_by_idx (value->list, lidx);
      memcpy (&tvalue, lvalue, sizeof (mkc_value_t));
      mkc_process_substitutions (process, &tvalue);
      mkc_list_set (nlist, &tvalue, sizeof (mkc_value_t), &loc);
    }

    value->list = nlist;
  }
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
mkc_process_temp_value_free (void *tvalue)
{
  mkc_value_t   *value = tvalue;

  if (value == NULL) {
    return;
  }

  if (value->vtype == MKC_VT_LIST ||
      value->vtype == MKC_VT_QUOTED_STRING ||
      value->vtype == MKC_VT_ENV_VARIABLE) {
    mkc_value_free (value);
  }
}

static void
mkc_process_topo_add_items (mkc_process_t *process,
    mkc_toposort_t *topo, mkc_list_t *hlist)
{
  mkc_listidx_t   hiteridx;
  mkc_listidx_t   hidx;

  mkc_list_iter_start (hlist, &hiteridx);
  while ((hidx = mkc_list_iter_next (hlist, &hiteridx)) != MKC_ITER_FINISH) {
    char        **temp;
    const char  *hdr;

    temp = mkc_list_get_by_idx (hlist, hidx);
    hdr = *temp;
    mkc_toposort_add_item (topo, hdr);
  }
}

static void
mkc_process_topo_add_deps (mkc_process_t *process,
    mkc_toposort_t *topo, char *rbuff, const char *hdr)
{
  mkc_list_t      *deplist;
  mkc_listidx_t   diteridx;
  mkc_listidx_t   didx;

  deplist = mkc_list_init (MKC_LIST_UNSORTED, mkc_list_ind_free, NULL, process->mkcerr);

  mkc_check_get_include_deps (process->check,
      process->attr.currcompiler, rbuff, deplist);

  mkc_list_iter_start (deplist, &diteridx);
  while ((didx = mkc_list_iter_next (deplist, &diteridx)) != MKC_ITER_FINISH) {
    char  **temp;
    char  *dep;

    temp = mkc_list_get_by_idx (deplist, didx);
    dep = *temp;
    mkc_toposort_add_pair (topo, hdr, dep);
  }

  mkc_list_free (deplist);
}

static mkc_list_t *
mkc_process_get_include_list (mkc_process_t *process, mkc_regex_t *rx)
{
  mkc_list_t      *hlist = NULL;
#if _have_regex
  mkc_listidx_t   piteridx;
  mkc_listidx_t   pathidx;

  mkc_list_iter_start (process->attr.pathlist, &piteridx);
  while ((pathidx = mkc_list_iter_next (process->attr.pathlist, &piteridx)) != MKC_ITER_FINISH) {
    mkc_value_t   *valpath = NULL;
    const char    *path = NULL;
    mkc_list_t    *tlist = NULL;
    mkc_listidx_t iteridx;
    mkc_listidx_t idx;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    valpath = mkc_list_get_by_idx (process->attr.pathlist, pathidx);
    path = valpath->sval;

    tlist = mkc_dir_match (path, rx, process->mkcerr);
    if (hlist == NULL) {
      hlist = tlist;
    } else {
      mkc_list_iter_start (tlist, &iteridx);
      while ((idx = mkc_list_iter_next (tlist, &iteridx)) != MKC_ITER_FINISH) {
        char          **temp;
        const char    *hdr;
        mkc_listidx_t loc;

        temp = mkc_list_get_by_idx (tlist, idx);
        hdr = strdup (*temp);
        mkc_list_set (hlist, &hdr, sizeof (char *), &loc);
      }

      mkc_list_free (tlist);
    }
  }

#endif
  return hlist;
}

static char *
mkc_process_iter_includes (mkc_process_t *process, mkc_list_t *hlist,
    mkc_listidx_t *hiteridx, char *hdr, size_t hsz, int readflag)
{
  mkc_listidx_t   hidx;
  char            *tbuff;
  char            *rbuff = NULL;

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  while ((hidx = mkc_list_iter_next (hlist, hiteridx)) != MKC_ITER_FINISH) {
    char            **temp;
    const char      *thdr;
    mkc_listidx_t   piteridx;
    mkc_listidx_t   pathidx;

    temp = mkc_list_get_by_idx (hlist, hidx);
    thdr = *temp;
    stpecpy (hdr, hdr + hsz, thdr);

    mkc_list_iter_start (process->attr.pathlist, &piteridx);
    while ((pathidx = mkc_list_iter_next (process->attr.pathlist, &piteridx)) != MKC_ITER_FINISH) {
      mkc_value_t   *valpath;
      const char    *path;
      size_t        fsz = 0;

      valpath = mkc_list_get_by_idx (process->attr.pathlist, pathidx);
      path = valpath->sval;

      snprintf (tbuff, MKC_PATH_MAX, "%s/%s", path, thdr);
      if (! mkc_file_exists (tbuff)) {
        continue;
      }

      if (readflag == MKC_INC_READ) {
        rbuff = mkc_read_file (tbuff, &fsz, process->mkcerr);
      }
      if (readflag == MKC_INC_NAME_ONLY) {
        /* point at something not-null to return */
        rbuff = hdr;
      }
      free (tbuff);
      return rbuff;
    }
  }

  free (tbuff);
  return NULL;
}

static char **
mkc_process_get_cflags (mkc_process_t *process)
{
  mkc_profidx_t   opidx;
  mkc_profiter_t  profiter;
  mkc_profidx_t   pidx;
  mkc_list_t      *tlist;
  mkc_listidx_t   psz;
  char            **cflags = NULL;
  int             cfsz = 0;
  int             cfallocsz = 0;

  opidx = mkc_profile_get_active (process->profiles);

  tlist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, process->mkcerr);

  /* to get the cflags, the profile hierarchy needs to be */
  /* traversed in reverse order */
  /* iterate the profile as usual, then traverse the list in reverse order */
  mkc_profile_iter_hierarchy_start (process->profiles, &profiter);
  while ((pidx = mkc_profile_iter_hierarchy_next (process->profiles, &profiter)) >= 0) {
    mkc_listidx_t   loc;

    mkc_list_set (tlist, &pidx, sizeof (mkc_profidx_t), &loc);
  }

  /* this is an unsorted list, the indices are in sequence */
  psz = mkc_list_size (tlist);
  for (int32_t i = psz - 1; i >= 0; --i) {
    mkc_profidx_t   *tpidx;
    mkc_listidx_t   cfiter;
    mkc_listidx_t   cfidx;
    mkc_value_t     *value;

    tpidx = mkc_list_get_by_idx (tlist, i);

    if (*tpidx == process->pidx_internal) {
      /* the internal profile will not have any cflags */
      continue;
    }

    mkc_pvar_profile_set_idx (process->pvar, *tpidx);
    value = mkc_pvar_get_by_profidx (process->pvar, MKC_C_CFLAGS, *tpidx);
    if (value == NULL || value->vtype != MKC_VT_LIST) {
      continue;
    }

    mkc_list_iter_start (value->list, &cfiter);
    while ((cfidx = mkc_list_iter_next (value->list, &cfiter)) != MKC_ITER_FINISH) {
      mkc_value_t   *cfval;

      cfval = mkc_list_get_by_idx (value->list, cfidx);

      if (cfsz >= cfallocsz) {
        cfallocsz += 10;
        /* always make room for a trailing NULL */
        cflags = realloc (cflags, sizeof (char *) * (cfallocsz + 1));
      }
      cflags [cfsz + 1] = NULL;
      cflags [cfsz] = strdup (cfval->sval);
      cfsz += 1;
    }
  }

  mkc_pvar_profile_set_idx (process->pvar, opidx);
  mkc_list_free (tlist);

  return cflags;
}

// ### make this generic, put into util...
void
mkc_process_free_flags (char **flags)
{
  char      *p;
  int32_t   c;

  if (flags == NULL) {
    return;
  }

  c = 0;
  p = flags [c];
  while (p != NULL) {
    free (p);
    ++c;
    p = flags [c];
  }
  free (flags);
}
