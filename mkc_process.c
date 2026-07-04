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

enum {
  MKC_AUTO_DEFINE_ZERO,
  MKC_AUTO_SKIP_ZERO,
};

enum {
  MKC_CACHE_VERS_1 = 1,
};

typedef struct mkc_user_regex_t {
  char          *pattern;
  mkc_regex_t   *rx;
} mkc_user_regex_t;

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

enum {
  MKC_NO_VARIADIC_MACRO = false,
  MKC_VARIADIC_MACRO_SUPPORTED = true,
};

static char const * const liblocname = "MKC_LIB_LOC_LIB64";
static char const * const shlibext = "MKC_SHARED_LIBRARY_EXTENSION";
static char const * const objext = "MKC_OBJECT_EXTENSION";
static char const * const exeext = "MKC_EXECUTABLE_EXTENSION";
static char const * const mkcwhilelimit = "MKC_TV_WHILE_LIMIT";
static char const * const mkctempvarpfx = "MKC_TV_";
static size_t mkctvpfxlen = 0;
static char const * const mkcivarmacro = "MKC_I_VARIADIC_MACRO";
static char const * const mkcprojectname = "MKC_PROJECT_NAME";
static char const * const mkcpathname = "MKC_PATH";
/* these are duplicated from mkc_profile.c */
/* so that the static aggregator can be initialized */
static char const * const mkcppkgconf = "MKC_PATH_PKGCONF";
static char const * const mkcppkgconfig = "MKC_PATH_PKG_CONFIG";

typedef struct mkc_prog_chk_t {
  const char  * program;
  const char  * mkcvarname;
} mkc_prog_chk_t;

/* these are executables that are used by mkc */
static mkc_prog_chk_t proglist [] = {
  { "pkgconf",      mkcppkgconf },
  { "pkg-config",   mkcppkgconfig },
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
static void mkc_process_find_executables (mkc_process_t *process);
static mkc_user_regex_t *mkc_process_user_regex_init (mkc_process_t *process, const char *pattern);
static void mkc_process_user_regex_free (void *turx);
static int mkc_process_user_regex_comp (void *turxa, void *turxb);
static void mkc_process_dbg_print_var (mkc_process_t *process, const char *pname);
static void mkc_process_dbg_print_prof (mkc_process_t *process);
static void mkc_process_dbg_print_path (mkc_process_t *process);
static void mkc_process_dbg_print_int_var (mkc_process_t *process);


MKC_NODISCARD
mkc_process_t *
mkc_process_init (mkc_profile_t *profiles, mkc_log_t *log,
    mkc_context_t *context, mkc_option_t *mkcoptions, mkc_error_t *mkcerr)
{
  mkc_process_t     *process;
  mkc_profidx_t     pidx;
  int               rc;

  mkctvpfxlen = strlen (mkctempvarpfx);
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
  process->user_rx_list = mkc_list_init (MKC_LIST_SORTED,
      mkc_process_user_regex_free, mkc_process_user_regex_comp, mkcerr);

  process->attr.currcompiler = process->dfltcompiler;
  process->attr.headertype = process->headertype;
  process->attr.hdrlist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, mkcerr);
  process->attr.compflags = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, mkcerr);
  process->attr.linkflags = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, mkcerr);
  process->attr.name = NULL;
  process->attr.method = NULL;
  process->attr.vcontext = NULL;
  process->attr.input = NULL;
  process->attr.output = NULL;
  process->attr.path = NULL;
  process->attr.negate = false;
  process->attr.define_zero = MKC_AUTO_SKIP_ZERO;

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
      MKC_PROF_INTERNAL_NAME, MKC_COMPILER_GENERAL);

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
  mkc_list_free (process->attr.hdrlist);
  mkc_list_free (process->attr.compflags);
  mkc_list_free (process->attr.linkflags);

  if (process->rxshellvar != NULL) {
#if _have_regex
    mkc_regex_free (process->rxshellvar);
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
  char        tbuff [MKC_VNAME_MAX];

  if (process == NULL) {
    return 0;
  }

  if (type != MKC_T_OP_IS_DEFINED && type != MKC_T_OP_IS_LIST) {
    ivala = mkc_pvar_value_get_integer (process->pvar, vala);
    if (mkc_error_chk_err (process->mkcerr)) {
      return 0;
    }
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
    case MKC_T_OP_IS_DEFINED: {
      mkc_pvar_value_get_str (process->pvar, vala, tbuff, sizeof (tbuff));
      result = mkc_pvar_is_defined (process->pvar, tbuff);
      break;
    }
    case MKC_T_OP_IS_LIST: {
      mkc_pvar_value_get_str (process->pvar, vala, tbuff, sizeof (tbuff));
      result = mkc_pvar_is_list (process->pvar, tbuff);
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
mkc_process_stmt_configure (mkc_process_t *process)
{
  int       defzero = MKC_AUTO_SKIP_ZERO;

  if (process->attr.method == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_PROC_NO_METHOD, 0, NULL);
    mkc_process_attr_clear (process);
    return;
  }

  defzero = process->attr.define_zero;

  if (strcmp (process->attr.method, "auto") == 0) {
    mkc_process_configure_auto (process, defzero);
  } else if (strcmp (process->attr.method, "manual") == 0) {
    if (process->attr.input == NULL) {
      mkc_error_set (process->mkcerr, MKC_ERR_PROC_NO_INPUT, 0, NULL);
      mkc_process_attr_clear (process);
      return;
    }
    if (process->attr.output == NULL) {
      mkc_error_set (process->mkcerr, MKC_ERR_PROC_NO_OUTPUT, 0, NULL);
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

void
mkc_process_stmt_project (mkc_process_t *process)
{
  mkc_profidx_t   pidx;

  if (process->attr.name == NULL ||
      *(process->attr.name) == '\0') {
    mkc_error_set (process->mkcerr, MKC_ERR_PROC_NO_NAME, 0, NULL);
    mkc_process_attr_clear (process);
    return;
  }

  datafree (process->projectname);
  process->projectname = strdup (process->attr.name);
  process->dfltcompiler = process->attr.currcompiler;

  pidx = mkc_profile_get_active (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);
  mkc_pvar_set_str (process->pvar, mkcprojectname, process->projectname, MKC_VCTXT_MKC);
  mkc_pvar_profile_set_idx (process->pvar, pidx);

  mkc_process_attr_clear (process);

  return;
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

int
mkc_process_stmt_set (mkc_process_t *process,
    mkc_value_t *valnm, mkc_value_t *value)
{
  char              nm [MKC_VNAME_MAX];
  char              buff [MKC_PATH_MAX];
  mkc_value_t       tvalue;
  mkc_value_t       *nvalue = NULL;
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

  if (value->vtype == MKC_VT_ENV_VARIABLE ||
      value->vtype == MKC_VT_QUOTED_STRING) {
    /* need to get the actual value */
    mkc_pvar_value_get_str (process->pvar, value, buff, sizeof (buff));
    tvalue.sval = buff;
    tvalue.vtype = MKC_VT_STRING;
    value = &tvalue;
  }
  if (value->vtype == MKC_VT_VARIABLE) {
    char    *tstr;

    /* substitution must be run on the variable before the value is fetched */
    tstr = mkc_pvar_substitute (process->pvar, value->sval, true, 0);
    if (tstr != NULL) {
      nvalue = mkc_process_get_value (process, tstr);
      value = nvalue;
      free (tstr);
    }
  }

  if (process->attr.vcontext != NULL) {
    const char    *tvc;

    tvc = process->attr.vcontext;
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

  if (value != NULL) {
    trc = mkc_pvar_set (process->pvar, nm, value, vctxt);
    if (trc == MKC_OK_CHANGE) {
      process->cacheinvalidated = true;
    }
  }

  mkc_process_attr_clear (process);
  return trc;
}

void
mkc_process_include (mkc_process_t *process, mkc_value_t *vala,
    char *tbuff, size_t sz)
{
  if (process == NULL) {
    return;
  }
  if (vala == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  *tbuff = '\0';
  mkc_pvar_value_get_str (process->pvar, vala, tbuff, sz);
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

  if (process == NULL) {
    return;
  }

  switch (iasttype) {
    case MKC_T_ATTR_CONTEXT: {
      ctxt = MKC_CONTEXT_SET;
      break;
    }
    case MKC_T_ATTR_DEFINE_ZERO: {
      ctxt = MKC_CONTEXT_CONFIGURE;
      break;
    }
    case MKC_T_ATTR_INPUT: {
      ctxt = MKC_CONTEXT_CONFIGURE;
      break;
    }
    case MKC_T_ATTR_METHOD: {
      ctxt = MKC_CONTEXT_CONFIGURE;
      break;
    }
    case MKC_T_ATTR_NAME: {
      ctxt = MKC_CONTEXT_CHECK | MKC_CONTEXT_COMP_FLAG | MKC_CONTEXT_PROJECT;
      break;
    }
    case MKC_T_ATTR_NEGATE: {
      ctxt = MKC_CONTEXT_COMP_FLAG;
      break;
    }
    case MKC_T_ATTR_OUTPUT: {
      ctxt = MKC_CONTEXT_CONFIGURE;
      break;
    }
    case MKC_T_ATTR_PATH: {
      ctxt = MKC_CONTEXT_CHECK;
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
    case MKC_T_ATTR_CONTEXT: {
      p = &process->attr.vcontext;
      break;
    }
    case MKC_T_ATTR_DEFINE_ZERO: {
      p = NULL;
      process->attr.define_zero = MKC_AUTO_DEFINE_ZERO;
      break;
    }
    case MKC_T_ATTR_INPUT: {
      p = &process->attr.input;
      break;
    }
    case MKC_T_ATTR_METHOD: {
      p = &process->attr.method;
      break;
    }
    case MKC_T_ATTR_NAME: {
      p = &process->attr.name;
      break;
    }
    case MKC_T_ATTR_NEGATE: {
      p = NULL;
      process->attr.negate = true;
      break;
    }
    case MKC_T_ATTR_OUTPUT: {
      p = &process->attr.output;
      break;
    }
    case MKC_T_ATTR_PATH: {
      p = &process->attr.path;
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
      MKC_CONTEXT_PROJECT | MKC_CONTEXT_PROFILE)) {
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

  if (strcmp (profnm, MKC_PROF_INTERNAL_NAME) == 0) {
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

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context, MKC_CONTEXT_CHECK)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  mkc_list_iter_start (value->list, &iteridx);
  while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    mkc_value_t     *lvalue;
    mkc_listidx_t   loc;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    mkc_list_set (process->attr.compflags, lvalue, sizeof (mkc_value_t), &loc);
  }

  return;
}

void
mkc_process_attr_header (mkc_process_t *process, mkc_value_t *value)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context, MKC_CONTEXT_CHECK)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  mkc_list_iter_start (value->list, &iteridx);
  while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    mkc_value_t     *lvalue;
    mkc_listidx_t   loc;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    mkc_list_set (process->attr.hdrlist, lvalue, sizeof (mkc_value_t), &loc);
  }

  return;
}

void
mkc_process_attr_link_flags (mkc_process_t *process, mkc_value_t *value)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;

  if (process == NULL) {
    return;
  }

  if (! mkc_context_check (process->context, MKC_CONTEXT_CHECK)) {
    mkc_error_set (process->mkcerr, MKC_ERR_STMT_NOT_ALLOWED, 0, NULL);
    return;
  }

  mkc_list_iter_start (value->list, &iteridx);
  while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    mkc_value_t     *lvalue;
    mkc_listidx_t   loc;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    mkc_list_set (process->attr.linkflags, lvalue, sizeof (mkc_value_t), &loc);
  }

  return;
}

mkc_value_t *
mkc_process_get_value (mkc_process_t *process, const char *nm)
{
  mkc_value_t   *value;

  value = mkc_pvar_get_by_profile (process->pvar, nm);
  return value;
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
  const char  *tmp;
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

  tmp = mkc_pvar_name_alloc (pvar, tnm);

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
      rc = mkc_chk_header (process->check, process->attr.currcompiler, txt);
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
    mkc_pvar_set_integer (pvar, tmp, rc == 0 ? true : false, MKC_VCTXT_CHECK);
    mkc_message ("-- check %s: %s : %s - %s\n",
        typenames [asttype], txt, tmp, mkc_success_msg (rc));
    mkc_log (process->log, MKC_LOG_CHECK, "-- check %s: %s : %s - %s\n",
        typenames [asttype], txt, tmp, mkc_success_msg (rc));
  }
  if (valtype) {
    mkc_pvar_set_integer (pvar, tmp, rc, MKC_VCTXT_CHECK);
    mkc_message ("-- check %s: %s : %s : %d\n", typenames [asttype], txt, tmp, rc);
    mkc_log (process->log, MKC_LOG_CHECK,
        "-- check %s: %s : %s : %d\n", typenames [asttype], txt, tmp, rc);
  }

  mkc_process_attr_clear (process);
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
    const char  *tmp;

    tmp = mkc_pvar_name_alloc (pvar, tnm);
    mkc_pvar_set_str (pvar, tmp, flag, MKC_VCTXT_FLAG);
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
mkc_process_chk_compiler_flag (mkc_process_t *process,
    mkc_value_t *valflag, int addchk)
{
  int         rc = MKC_OK;
  char        tnm [MKC_VNAME_MAX];
  char        flag [MKC_VNAME_MAX];
  mkc_pvar_t  *pvar;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  mkc_pvar_value_get_str (process->pvar, valflag, flag, sizeof (flag));
  mkc_process_create_name (process, MKC_T_CHK_COMP_FLAG, tnm, sizeof (tnm), "cf_", flag, NULL);

  pvar = process->pvar;

  if (mkc_process_chk_cache (process, flag, tnm)) {
    mkc_process_attr_clear (process);
    return rc;
  }

  if (addchk == MKC_CHK) {
    rc = mkc_chk_compiler_flag (process->check,
        process->attr.currcompiler, flag, process->attr.negate);
  }
  process->attr.negate = false;

  if (rc == 0) {
    const char  *tmp;

    tmp = mkc_pvar_name_alloc (pvar, tnm);
    mkc_pvar_set_str (pvar, tmp, flag, MKC_VCTXT_FLAG);
  }

  if (addchk == MKC_ADD) {
    mkc_message ("-- add compiler flag: %s\n", flag);
    mkc_log (process->log, MKC_LOG_CHECK,
        "-- add compiler flag: %s\n", flag);
  }
  if (addchk == MKC_CHK) {
    mkc_message ("-- check compiler flag: %s - %s\n",
        flag, mkc_success_msg (rc));
    mkc_log (process->log, MKC_LOG_CHECK, "-- check compiler flag: %s - %s\n",
        flag, mkc_success_msg (rc));
  }

  mkc_process_attr_clear (process);
  return rc;
}

int32_t
mkc_process_chk_link_flag (mkc_process_t *process,
    mkc_value_t *valflag, int addchk)
{
  int         rc = MKC_OK;
  char        tnm [MKC_VNAME_MAX];
  char        flag [MKC_VNAME_MAX];
  mkc_pvar_t  *pvar;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  pvar = process->pvar;
  mkc_pvar_value_get_str (process->pvar, valflag, flag, sizeof (flag));
  mkc_process_create_name (process, MKC_T_CHK_LINK_FLAG, tnm, sizeof (tnm), "lf_", flag, NULL);

  if (mkc_process_chk_cache (process, flag, tnm)) {
    mkc_process_attr_clear (process);
    return rc;
  }

  if (addchk == MKC_CHK) {
    rc = mkc_chk_link_flag (process->check,
        process->attr.currcompiler, flag);
  }
  if (rc == 0) {
    const char  *tmp;

    tmp = mkc_pvar_name_alloc (pvar, tnm);
    mkc_pvar_set_str (pvar, tmp, flag, MKC_VCTXT_FLAG);
  }

  if (addchk == MKC_ADD) {
    mkc_message ("-- add link flag: %s\n", flag);
    mkc_log (process->log, MKC_LOG_CHECK,
        "-- add link flag: %s\n", flag);
  }
  if (addchk == MKC_CHK) {
    mkc_message ("-- check link flag: %s - %s\n",
        flag, mkc_success_msg (rc));
    mkc_log (process->log, MKC_LOG_CHECK, "-- check link flag: %s - %s\n",
        flag, mkc_success_msg (rc));
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
  const char  *tmp;
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
  tmp = mkc_pvar_name_alloc (pvar, tnm);
  mkc_pvar_set_integer (pvar, tmp, rc == 0 ? true : false, MKC_VCTXT_CHECK);

  mkc_message ("-- check struct member: %s.%s - %s\n",
      structname, membername, mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check struct member: %s.%s - %s\n",
      structname, membername, mkc_success_msg (rc));

  mkc_process_attr_clear (process);
  return rc;
}

void
mkc_process_chk_shell_extract (mkc_process_t *process, mkc_value_t *valpath)
{
#if _have_regex
  FILE        *fh;
  char        buff [MKC_VNAME_MAX];
  char        *path;
  char        varname [MKC_VNAME_MAX];
  char        *varvalue;
  char        *tvalue;

  if (process == NULL) {
    return;
  }

  path = malloc (MKC_PATH_MAX);
  if (path == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  mkc_pvar_value_get_str (process->pvar, valpath, path, MKC_PATH_MAX);

  fh = mkc_fopen (path, "r");
  if (fh == NULL) {
    return;
  }

  if (process->rxshellvar == NULL) {
    process->rxshellvar = mkc_regex_init (
      "^[ \t]*([[:alnum:]_]+)=((\"(([^\"\\\\]|\\\\.)*)\")|([^ \t\r\n]*))$", process->mkcerr);
    /*  0: entire string */
    /*  1: var-name */
    /*  2: "..." or ... */
    /*  3: "..." */
    /*  4: ... (inside of quotes) */
    /*  5: letter (inside of quotes) */
    /*  6: ... (no quotes) */
    if (mkc_error_chk_err (process->mkcerr)) {
      return;
    }
  }

  varvalue = malloc (MKC_PATH_MAX);
  if (varvalue == NULL) {
    free (path);
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  while (fgets (buff, sizeof (buff), fh) != NULL) {
    char      **match;
    int       matchcount;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    mkc_strtrim (buff, sizeof (buff));
    mkc_log (process->log, MKC_LOG_CHECK, "  shell: buff: %s\n", buff);

    match = mkc_regex_get (process->rxshellvar, buff, &matchcount);
    if (match == NULL) {
      continue;
    }

    *varname = '\0';
    *varvalue = '\0';

    mkc_log (process->log, MKC_LOG_CHECK, "  shell: matchcount: %d\n", matchcount);
    if (matchcount != 6 && matchcount != 7) {
      continue;
    }

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

    tvalue = mkc_pvar_substitute (process->pvar, varvalue, true, 0);
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

  fclose (fh);
  free (path);
  free (varvalue);
#endif

  mkc_process_attr_clear (process);
  return;
}

void
mkc_process_local_set (mkc_process_t *process, const char *nm,
    const char *sval, mkc_profidx_t pidx)
{
  mkc_profidx_t   opidx;

  if (process == NULL) {
    return;
  }
  if (nm == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  opidx = mkc_profile_get_active (process->profiles);
  mkc_profile_local_reset (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, pidx);
  mkc_pvar_set_str (process->pvar, nm, sval, MKC_VCTXT_TEMP);
  mkc_pvar_profile_set_idx (process->pvar, opidx);
}

int32_t
mkc_process_get_while_limit (mkc_process_t *process)
{
  int32_t     limit = 10000;
  mkc_value_t *value;

  if (process == NULL) {
    return limit;
  }

  value = mkc_pvar_get_by_profile (process->pvar, mkcwhilelimit);
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
      if (strcmp (nm, MKC_PROF_INTERNAL_NAME) != 0) {
        fprintf (fh, "    compiler %s;\n", compname);
      }
      indent = "  ";
    }

    mkc_pvar_iter_start (pvar, &viter);
    while ((vidx = mkc_pvar_iter_next (pvar, &viter)) != MKC_ITER_FINISH) {
      const char    *nm;
      mkc_value_t   *value;

      nm = mkc_pvar_get_name (pvar, vidx);

      /* temporary variables do not need to be cached */
      if (strncmp (nm, mkctempvarpfx, mkctvpfxlen) == 0) {
        continue;
      }

      value = mkc_pvar_get_by_idx (pvar, vidx);

      if (value->vtype == MKC_VT_LIST) {
        mkc_list_t    *list;
        mkc_listidx_t lidx;
        mkc_listidx_t iteridx;
        const char    *vctxtstr = "";

        list = value->list;
        mkc_list_iter_start (list, &iteridx);
        fprintf (fh, "  %sset %s [ ", indent, nm);
        while ((lidx = mkc_list_iter_next (list, &iteridx)) != MKC_ITER_FINISH) {
          mkc_value_t   *tvalue;

          if (mkc_error_chk_err (process->mkcerr)) {
            break;
          }
          tvalue = mkc_list_get_by_idx (list, lidx);
          if (tvalue->vtype == MKC_VT_INTEGER) {
            fprintf (fh, "%d ", tvalue->ival);
          }
          if (tvalue->vtype != MKC_VT_INVALID &&
              tvalue->vtype != MKC_VT_INTEGER &&
              tvalue->vtype != MKC_VT_LIST) {
            fprintf (fh, "'%s' ", tvalue->sval);
          }
          if (tvalue->vtype == MKC_VT_LIST) {
// ### argh, will this be valid?
          }
        }
        vctxtstr = mkc_var_vctxt_str (value->vctxt);
        fprintf (fh, "] { context %s; }\n", vctxtstr);
        ++count;
        ++tcount;
      }
      if (value->vtype == MKC_VT_STRING) {
        const char  *val;
        const char  *vctxtstr = "";

        val = value->sval;
        vctxtstr = mkc_var_vctxt_str (value->vctxt);
        fprintf (fh, "  %sset %s '%s' { context %s; }\n",
            indent, nm, val, vctxtstr);
        ++count;
        ++tcount;
      }
      if (value->vtype == MKC_VT_INTEGER) {
        int32_t     ival;
        const char  *vctxtstr;

        ival = value->ival;
        vctxtstr = mkc_var_vctxt_str (value->vctxt);
        fprintf (fh, "  %sset %s %d { context %s; }\n",
            indent, nm, ival, vctxtstr);
        ++count;
        ++tcount;
      }
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
  fclose (fh);
}

/* internal routines */

const char *
mkc_process_create_name (mkc_process_t *process, mkc_astnode_token_t asttype,
    char *buff, size_t sz, const char *tag, ...)
{
  char        *p;
  size_t      len;
  size_t      nlen = 0;
  const char  * str;
  va_list     ap;

  va_start (ap, tag);

  /* for chk-package, the name replaces the name of the package */
  if (process->attr.name != NULL && asttype != MKC_T_CHK_PACKAGE) {
    stpecpy (buff, buff + sz, process->attr.name);
    va_end (ap);
    return buff;
  }

  p = stpecpy (buff, buff + sz, tag);
  nlen = strlen (buff);

  /* the caller must pass in a NULL terminal indicator */
  while ((str = va_arg (ap, const char *)) != NULL) {
    if (process->attr.name != NULL && asttype == MKC_T_CHK_PACKAGE) {
      /* special case for chk-package */
      str = process->attr.name;
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

  /* modern header support : dflt/comp */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_curr_comp);

  rc = mkc_chk_header_modern (process->check, process->dfltcompiler);
  if (rc != 0) {
    process->headertype = MKC_HEADER_LEGACY;
  }
  process->attr.headertype = process->headertype;
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", "header-type", process->headertype);

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

  /* object, executable extension : internal */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  if (process->systype == MKC_SYS_WINDOWS) {
    process->objext = ".obj";
    process->exeext = ".exe";
  }
  mkc_pvar_set_str (process->pvar, objext, process->objext, MKC_VCTXT_MKC);
  mkc_pvar_set_str (process->pvar, exeext, process->exeext, MKC_VCTXT_MKC);

  /* shared library extension : internal */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  /* default is .so */
  mkc_pvar_set_str (process->pvar, shlibext, ".so", MKC_VCTXT_MKC);
  isystype = process->systype;
  switch (isystype) {
    case MKC_SYS_AIX: {
      mkc_pvar_set_str (process->pvar, shlibext, ".a", MKC_VCTXT_MKC);
      break;
    }
    case MKC_SYS_MACOS: {
      mkc_pvar_set_str (process->pvar, shlibext, ".dylib", MKC_VCTXT_MKC);
      break;
    }
    case MKC_SYS_WINDOWS: {
      mkc_pvar_set_str (process->pvar, shlibext, ".dll", MKC_VCTXT_MKC);
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

  /* variadic macro support : dflt/comp */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_curr_comp);

  rc = mkc_chk_variadic_macro (process->check, process->dfltcompiler);
  if (rc != 0) {
    process->variadicmacro = MKC_NO_VARIADIC_MACRO;
  }
  mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", mkcivarmacro, process->variadicmacro);
  mkc_pvar_set_integer (process->pvar, mkcivarmacro, process->variadicmacro, MKC_VCTXT_MKC);

  /* linux: library location : internal */

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  if (process->systype == MKC_SYS_LINUX) {
    rc = mkc_chk_library_location (process->check, process->dfltcompiler);
    if (rc >= 0) {
      process->libloc = rc;
    }
    mkc_log (process->log, MKC_LOG_GENERAL, "%s: %d\n", liblocname, process->libloc);
    mkc_pvar_set_integer (process->pvar, liblocname, process->libloc, MKC_VCTXT_MKC);
  }

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

  mkc_pvar_set_integer (process->pvar, mkcwhilelimit, 10000, MKC_VCTXT_MKC);
  mkc_pvar_set_integer (process->pvar, liblocname, process->libloc, MKC_VCTXT_MKC);

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

  mkc_pvar_set_str (process->pvar, mkcprofilename,
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

  data = mkc_read_file (process->attr.input, &fsz, process->mkcerr);
  if (mkc_error_chk_err (process->mkcerr)) {
    mkc_error_set (process->mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, process->attr.input);
    return;
  }
  ndata = mkc_pvar_substitute (process->pvar, data, true, 0);
  free (data);
  fh = mkc_fopen (process->attr.output, "wb");
  if (fh == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, process->attr.output);
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
  char            fname [MKC_PATH_MAX];
  char            *tp;
  char            projnm [MKC_VNAME_MAX];
  size_t          len;

  tp = process->projectname;
  if (tp == NULL) {
    tp = "project";
  }
  tp = stpecpy (projnm, projnm + sizeof (projnm), tp);
  tp = stpecpy (tp, projnm + sizeof (projnm), "_config");

  if (process->attr.output != NULL) {
    stpecpy (fname, fname + sizeof (fname), process->attr.output);
    tp = strrchr (fname, '/');
    if (tp != NULL) {
      tp = stpecpy (projnm, projnm + sizeof (projnm), tp + 1);
      tp = strrchr (projnm, '.');
      if (tp != NULL) {
        *tp = '\0';
      }
    }
  } else {
    snprintf (fname, sizeof (fname), "%s.h", projnm);
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

      if (value->vtype == MKC_VT_STRING) {
        const char  *val;

        val = value->sval;
        fprintf (fh, "#define %s \"%s\"\n", nm, val);
      }
      if (value->vtype == MKC_VT_INTEGER) {
        int32_t     ival;

        ival = value->ival;
        if (defzero == MKC_AUTO_DEFINE_ZERO || ival != 0) {
          fprintf (fh, "#define %s %d\n", nm, ival);
        }
      }
    }
  }

  fprintf (fh, "\n");
  fprintf (fh, "#endif /* INC_%s_H */\n", projnm);

  mkc_pvar_profile_set_idx (process->pvar, opidx);
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
mkc_process_find_executables (mkc_process_t *process)
{
  mkc_profidx_t   opidx;
  char            *tbuff;
  char            *testpath;
  char            *tpath;
  mkc_prog_chk_t  *chk;
  char            *p;
  mkc_list_t      *pathlist;
  mkc_listidx_t   loc;
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;
  char            *tokstr;
  const char      *pathdelim = ":";


  opidx = mkc_profile_get_active (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);

  if (process->systype == MKC_SYS_WINDOWS) {
    pathdelim = ";";
  }

  pathlist = mkc_list_init (MKC_LIST_UNSORTED,
      mkc_list_ind_free, NULL, process->mkcerr);
  if (mkc_error_chk_err (process->mkcerr)) {
    return;
  }

  tbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (tbuff == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  testpath = malloc (MKC_PATH_MAX);
  if (testpath == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  mkc_env_get ("PATH", tbuff, MKC_SMALL_BUFF_SZ);

  tpath = mkc_strtok (tbuff, pathdelim, &tokstr);
  while (tpath != NULL) {
    char    *tmp;

    tmp = strdup (tpath);
    mkc_normalize_path (tmp, strlen (tmp));
    mkc_list_append (pathlist, &tmp, sizeof (char *), &loc);
    tpath = mkc_strtok (NULL, pathdelim, &tokstr);
  }

  chk = proglist;
  while (chk->program != NULL) {
    mkc_list_iter_start (pathlist, &iteridx);
    while ((lidx = mkc_list_iter_next (pathlist, &iteridx)) != MKC_ITER_FINISH) {
      char    **tmp;

      tmp = mkc_list_get_by_idx (pathlist, lidx);
      tpath = *tmp;

      p = stpecpy (testpath, testpath + MKC_PATH_MAX, tpath);
      p = stpecpy (p, testpath + MKC_PATH_MAX, "/");
      p = stpecpy (p, testpath + MKC_PATH_MAX, chk->program);
      p = stpecpy (p, testpath + MKC_PATH_MAX, process->exeext);

      /* using file-size as an existence check */
      if (mkc_file_size (testpath) > 0) {
        mkc_pvar_set_str (process->pvar, chk->mkcvarname, testpath, MKC_VCTXT_MKC);
        break;
      }
    }
    chk += 1;
  }

  mkc_pvar_profile_set_idx (process->pvar, process->pidx_internal);
  mkc_pvar_set_list (process->pvar, mkcpathname, pathlist, MKC_VCTXT_ENV);

  free (tbuff);
  free (testpath);
  /* pathlist has been copied by var-set */
  mkc_list_free (pathlist);

  mkc_pvar_profile_set_idx (process->pvar, opidx);
}

static void
mkc_process_attr_clear (mkc_process_t *process)
{
  mkc_list_free (process->attr.hdrlist);
  process->attr.hdrlist = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, process->mkcerr);
  mkc_list_free (process->attr.compflags);
  process->attr.compflags = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, process->mkcerr);
  mkc_list_free (process->attr.linkflags);
  process->attr.linkflags = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, process->mkcerr);
  datafree (process->attr.name);
  process->attr.name = NULL;
  datafree (process->attr.method);
  process->attr.method = NULL;
  datafree (process->attr.vcontext);
  process->attr.vcontext = NULL;
  datafree (process->attr.input);
  process->attr.input = NULL;
  datafree (process->attr.output);
  process->attr.output = NULL;
  datafree (process->attr.path);
  process->attr.path = NULL;
  process->attr.define_zero = MKC_AUTO_SKIP_ZERO;
  process->attr.currcompiler = process->dfltcompiler;
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
  turx.rx = mkc_regex_init (turx.pattern, process->mkcerr);
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

  pvar = process->pvar;
  opidx = mkc_profile_get_active (process->profiles);

  if (pname != NULL && strcmp (pname, "default") == 0) {
    pname = mkc_profile_get_name (process->profiles, opidx);
  }
  if (pname != NULL && strcmp (pname, "test") == 0) {
    intest = true;
    pname = mkc_profile_get_name (process->profiles, opidx);
  }
  if (pname != NULL && strcmp (pname, "internalall") == 0) {
    pname = MKC_PROF_INTERNAL_NAME;
  }
  if (pname != NULL && strcmp (pname, "internal") == 0) {
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
        if (strncmp (nm, mkctempvarpfx, mkctvpfxlen) == 0) {
          continue;
        }
      }

      if (intest) {
        if (strcmp (nm, "CC") == 0 ||
            strcmp (nm, "CXX") == 0 ||
            strcmp (nm, "OBJC") == 0 ||
            strcmp (nm, "BISON") == 0 ||
            strcmp (nm, "FLEX") == 0 ||
            strcmp (nm, mkcivarmacro) == 0) {
          continue;
        }
      }

      if (value->vtype == MKC_VT_LIST) {
        mkc_list_t    *list;
        mkc_listidx_t lidx;
        mkc_listidx_t iteridx;

        list = value->list;
        mkc_list_iter_start (list, &iteridx);
        fprintf (stdout, "  %s [ ", nm);
        while ((lidx = mkc_list_iter_next (list, &iteridx)) != MKC_ITER_FINISH) {
          mkc_value_t   *tvalue;

          if (mkc_error_chk_err (process->mkcerr)) {
            break;
          }
          tvalue = mkc_list_get_by_idx (list, lidx);
          if (tvalue->vtype == MKC_VT_INTEGER) {
            fprintf (stdout, "%d ", tvalue->ival);
          }
          if (tvalue->vtype != MKC_VT_INVALID &&
              tvalue->vtype != MKC_VT_INTEGER &&
              tvalue->vtype != MKC_VT_LIST) {
            fprintf (stdout, "'%s' ", tvalue->sval);
          }
          if (tvalue->vtype == MKC_VT_LIST) {
// ### don't know yet if this will be legal
            fprintf (stdout, "[...] ");
          }
        }
        fprintf (stdout, "]\n");
      }
      if (value->vtype == MKC_VT_STRING) {
        const char  *val;

        val = value->sval;
        fprintf (stdout, "  %s %s\n", nm, val);
      }
      if (value->vtype == MKC_VT_INTEGER) {
        int32_t     ival;

        ival = value->ival;
        fprintf (stdout, "  %s %d\n", nm, ival);
      }
    }
  }

  mkc_pvar_profile_set_idx (process->pvar, opidx);
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
    fprintf (stdout, "path %d %s\n", i, tbuff);
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
