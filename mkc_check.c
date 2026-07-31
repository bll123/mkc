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
#include "mkc_check.h"
#include "mkc_compiler.h"
#include "mkc_const.h"
#include "mkc_def.h"
#include "envutil.h"
#include "mkc_error.h"
#include "fileop.h"
#include "mkc_log.h"
#include "os_process.h"
#include "pathutil.h"
#include "mkc_regex.h"
#include "strutil.h"
#include "scopedvar.h"
#include "tmutil.h"
#include "value.h"

#define MKC_PKG_TRACE 0

typedef struct mkc_check_t {
  scopedvar_t       * scopedvar;
  comptest_t        * comptest;
  mkc_error_t       * mkcerr;
  mkc_log_t         * log;
  mkc_attribute_t   * attr;
  char              * pkgname;
  mkc_regex_t       * rxargcount;
  mkc_regex_t       * rxcomma;
  mkc_regex_t       * rxincludedep;
} mkc_check_t;

static mkc_err_code_t mkc_chk_env_var_set (mkc_check_t *check, const char *nm);
static int mkc_chk_package_exec (mkc_check_t *check, const char *pkg);

MKC_NODISCARD
mkc_check_t *
mkc_check_init (scopedvar_t *scopedvar, comptest_t *comptest,
    mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr)
{
  mkc_check_t   *check;

  check = malloc (sizeof (mkc_check_t));
  check->scopedvar = scopedvar;
  check->comptest = comptest;
  check->attr = attr;
  check->mkcerr = mkcerr;
  check->log = log;
  check->pkgname = NULL;
  check->rxargcount = NULL;
  check->rxcomma = NULL;
  check->rxincludedep = NULL;

  return check;
}

void
mkc_check_free (mkc_check_t *check)
{
  if (check == NULL) {
    return;
  }

  datafree (check->pkgname);
  if (check->rxargcount != NULL) {
#if _have_regex
    mkc_regex_free (check->rxargcount);
#endif
  }
  if (check->rxcomma != NULL) {
#if _have_regex
    mkc_regex_free (check->rxcomma);
#endif
  }
  if (check->rxincludedep != NULL) {
#if _have_regex
    mkc_regex_free (check->rxincludedep);
#endif
  }

  free (check);
}

int
mkc_create_dirs (void)
{
  return 0;
}

mkc_err_code_t
mkc_chk_compiler_env (mkc_check_t *check)
{
  mkc_err_code_t    rc = MKC_OK;
  mkc_err_code_t    trc;

  trc = mkc_chk_env_var_set (check, "BISON");
  if (trc == MKC_OK_CHANGE) { rc = trc; }
  trc = mkc_chk_env_var_set (check, "CC");
  if (trc == MKC_OK_CHANGE) { rc = trc; }
  trc = mkc_chk_env_var_set (check, "CXX");
  if (trc == MKC_OK_CHANGE) { rc = trc; }
  trc = mkc_chk_env_var_set (check, "FLEX");
  if (trc == MKC_OK_CHANGE) { rc = trc; }
  trc = mkc_chk_env_var_set (check, "OBJC");
  if (trc == MKC_OK_CHANGE) { rc = trc; }

  return rc;
}

int
mkc_chk_compiler_works (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;

  /* clang prints the deprecated error when compiling C with */
  /* c++ or objective-c */
  mkc_chk_append_comp_flag (check, "-Wno-deprecated");

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: compiler-works\n");
  rc = comptest_test (check->comptest, MKC_COMPILE_ONLY, compiler,
      "int-main", NULL, 0);
  return rc;
}

void
mkc_chk_append_comp_flag (mkc_check_t *check, const char *flag)
{
  value_t           tvalue;
  mkc_alternate_t   * alt;

  if (check == NULL || flag == NULL) {
    return;
  }

  value_init (&tvalue);
  tvalue.sval = (char *) flag;
  tvalue.vtype = MKC_VT_STRING;
  alt = check->attr->curralt;
  mkc_list_set (alt->compflags, &tvalue, sizeof (value_t));
}

void
mkc_chk_append_link_flag (mkc_check_t *check, const char *flag)
{
  value_t           tvalue;
  mkc_alternate_t   * alt;

  if (check == NULL || flag == NULL) {
    return;
  }

  value_init (&tvalue);
  tvalue.sval = (char *) flag;
  tvalue.vtype = MKC_VT_STRING;
  alt = check->attr->curralt;
  mkc_list_set (alt->linkflags, &tvalue, sizeof (value_t));
}

int
mkc_chk_header_modern (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: header-modern\n");
  rc = comptest_test (check->comptest, MKC_COMPILE_ONLY, compiler,
      "int-header-modern", NULL, 0);
  return rc;
}

int
mkc_chk_system_type (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;
  char        *inc;
  const char  *flags [3];
  int         fcount = 0;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: system-type\n");

  inc = malloc (MKC_PATH_MAX);
  if (inc == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_SYS_UNKNOWN;
  }
  path_build (MKC_PATH_MKC_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  comptest_set_flags (check->comptest, flags, NULL, NULL);
  rc = comptest_test (check->comptest, MKC_COMPILE_RUN, compiler,
      "int-system", NULL, 0);
  comptest_reset (check->comptest);
  free (inc);
  return rc;
}

int
mkc_chk_system_id (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;
  char        *inc;
  const char  *flags [3];
  int         fcount = 0;

  inc = malloc (MKC_PATH_MAX);
  if (inc == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_SYS_ID_NOTSET;
  }
  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: system-id\n");
  path_build (MKC_PATH_MKC_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  comptest_set_flags (check->comptest, flags, NULL, NULL);
  rc = comptest_test (check->comptest, MKC_COMPILE_RUN, compiler,
      "int-sysid", NULL, 0);
  comptest_reset (check->comptest);
  free (inc);
  return rc;
}

int
mkc_chk_variadic_macro (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: variadic-macro\n");
  rc = comptest_test (check->comptest, MKC_COMPILE_RUN, compiler,
      "int-variadic-macro", NULL, 0);
  comptest_reset (check->comptest);
  return rc;
}

/* the library location is used for linux systems */
/* some linux systems use lib64 as the main library suffix */
/* other linux systems have lib64, but only use it for lib64 specific */
/* libraries */
int
mkc_chk_library_location (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;
  char        *inc;
  const char  *flags [3];
  int         fcount = 0;

  inc = malloc (MKC_PATH_MAX);
  if (inc == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return 0;
  }
  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: lib-location\n");
  path_build (MKC_PATH_MKC_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  comptest_set_flags (check->comptest, flags, NULL, NULL);
  rc = comptest_test (check->comptest, MKC_COMPILE_RUN, compiler,
      "int-libloc", NULL, 0);
  comptest_reset (check->comptest);
  free (inc);
  return rc;
}

int
mkc_chk_compiler_id (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;
  char        *inc;
  const char  *flags [3];
  int         fcount = 0;

  inc = malloc (MKC_PATH_MAX);
  if (inc == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return 0;
  }
  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: compiler-id\n");
  path_build (MKC_PATH_MKC_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  comptest_set_flags (check->comptest, flags, NULL, NULL);
  rc = comptest_test (check->comptest, MKC_COMPILE_RUN, compiler,
      "int-compid", NULL, 0);
  comptest_reset (check->comptest);
  free (inc);
  return rc;
}

int
mkc_chk_arg_count (mkc_check_t *check, mkc_compiler_t compiler,
    const char *funcname)
{
  int             rc = 0;
  char            *rbuff;
  size_t          rsz = MKC_LARGE_BUFF_SZ;
  const char      *flags [2];
  int             fcount = 0;
#if _have_regex
  char            pattern [MKC_VNAME_MAX];
  char            **match;
  int             matchcount = 0;
#endif

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: arg_count: %s\n", funcname);

  scopedvar_set_str (check->scopedvar, SV_T_LOCAL, "MKC_TV_TEST_FUNCTION_NAME", funcname, MKC_VCTXT_TEMP);

  rbuff = malloc (rsz);
  if (rbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_OK;
  }

  flags [fcount++] = "-E";
  flags [fcount++] = NULL;
  comptest_set_flags (check->comptest, flags, NULL, NULL);
  rc = comptest_test (check->comptest, MKC_COMPILE_ONLY, compiler,
      "c-argcount", rbuff, rsz);
  comptest_reset (check->comptest);

  /*  int mkdir (const char *__path, __mode_t __mode) */
  /*      ;   */

#if _have_regex
  if (check->rxcomma == NULL) {
    check->rxcomma = mkc_regex_init ("(,)", MKC_REGEX_NONE, check->mkcerr);
    if (mkc_error_chk_err (check->mkcerr)) {
      free (rbuff);
      return MKC_ERR_FAILURE;
    }
  }

  /* the function name changes, the pattern must be re-built */
  snprintf (pattern, sizeof (pattern),
      "([ \t*]+%s[ \t]*\\([^)]*\\)[ \t\r\n]*;)", funcname);
  mkc_log (check->log, MKC_LOG_CHECK, "  arg-count: pattern: %s\n", pattern);
  check->rxargcount = mkc_regex_init (pattern, MKC_REGEX_NONE, check->mkcerr);
  if (mkc_error_chk_err (check->mkcerr)) {
    free (rbuff);
    return MKC_ERR_FAILURE;
  }

  match = mkc_regex_get (check->rxargcount, rbuff, &matchcount);
  mkc_log (check->log, MKC_LOG_CHECK, "  arg-count: matches: %d\n", matchcount);
  if (matchcount == 2) {
    const char  *tmatch;

    /* now count the number of commas */
    tmatch = match [1];
    mkc_log (check->log, MKC_LOG_CHECK, "  arg-count: match: %s\n", tmatch);
    matchcount = mkc_regex_match_count (check->rxcomma, tmatch);
    mkc_log (check->log, MKC_LOG_CHECK, "  arg-count: commas: %d\n", matchcount);

    rc = matchcount + 1;
  }

  mkc_regex_get_free (match);
  mkc_regex_free (check->rxargcount);
  check->rxargcount = NULL;
#endif

  free (rbuff);
  return rc;
}

int
mkc_chk_compiler_flag (mkc_check_t *check,
    mkc_compiler_t compiler,
    const char *flag, bool negate)
{
  int               rc;
  char              tbuff [MKC_VNAME_MAX];
  char              *rbuff;
  size_t            rsz;
  static const char *negprefix = "-Wno-";
  static size_t     neglen = 5;

  rsz = MKC_SMALL_BUFF_SZ;
  rbuff = malloc (rsz);
  if (rbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }
  *rbuff = '\0';

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: compiler-flag: %s\n", flag);
  stpecpy (tbuff, tbuff + sizeof (tbuff), flag);
  if (negate == true) {
    char    *p;

    if (strncmp (flag, negprefix, neglen) == 0) {
      p = stpecpy (tbuff, tbuff + sizeof (tbuff), "-W");
      p = stpecpy (p, tbuff + sizeof (tbuff), flag + neglen);
    }
  }

  mkc_chk_append_comp_flag (check, tbuff);
  comptest_set_flags (check->comptest, NULL, NULL, NULL);
  rc = comptest_test (check->comptest, MKC_COMPILE_ONLY, compiler,
      "c-main", rbuff, rsz);
  comptest_reset (check->comptest);
  if (rc == 0) {
    /* clang does not return an error code on a unknown warning */
    if (strstr (rbuff, "warning") != NULL) {
      rc = 1;
    }
  }

  free (rbuff);
  return rc;
}

int
mkc_chk_const (mkc_check_t *check,
    mkc_compiler_t compiler, const char *consttxt)
{
  int             rc;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: constant: %s\n", consttxt);

  scopedvar_set_str (check->scopedvar, SV_T_LOCAL, "MKC_TV_TEST_CONSTANT", consttxt, MKC_VCTXT_TEMP);

  rc = comptest_test (check->comptest, MKC_COMPILE_ONLY, compiler,
      "c-const", NULL, 0);
  comptest_reset (check->comptest);
  return rc;
}

int
mkc_chk_define (mkc_check_t *check,
    mkc_compiler_t compiler, const char *def)
{
  int             rc;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: define: %s\n", def);

  scopedvar_set_str (check->scopedvar, SV_T_LOCAL, "MKC_TV_TEST_DEFINE", def, MKC_VCTXT_TEMP);

  rc = comptest_test (check->comptest, MKC_COMPILE_ONLY, compiler,
      "c-define", NULL, 0);
  comptest_reset (check->comptest);
  return rc;
}

int
mkc_chk_package (mkc_check_t *check,
    mkc_compiler_t compiler, const char *pkg)
{
  int             rc = MKC_ERR_FAILURE;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: package: %s\n", pkg);

  datafree (check->pkgname);
  check->pkgname = strdup (pkg);
  /* libpkgconf's api is far to complex to bother using. */
  rc = mkc_chk_package_exec (check, pkg);

  return rc;
}

int
mkc_chk_link_flag (mkc_check_t *check,
    mkc_compiler_t compiler, const char *flag)
{
  int               rc;
  char              *rbuff;

  rbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (rbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  mkc_chk_append_link_flag (check, flag);
  mkc_log (check->log, MKC_LOG_CHECK, "== chk: link-flag: %s\n", flag);
  rc = comptest_test (check->comptest, MKC_COMPILE_LINK, compiler,
      "c-main", rbuff, MKC_PATH_MAX);
  comptest_reset (check->comptest);
  if (rc == 0) {
    /* clang does not return an error code on a unknown warning */
    if (strstr (rbuff, "warning") != NULL) {
      rc = 1;
    }
  }
  free (rbuff);
  return rc;
}

int
mkc_chk_size (mkc_check_t *check,
    mkc_compiler_t compiler, const char *type)
{
  int             rc;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: size: %s\n", type);

  scopedvar_set_str (check->scopedvar, SV_T_LOCAL, "MKC_TV_TEST_SIZE", type, MKC_VCTXT_TEMP);

  rc = comptest_test (check->comptest, MKC_COMPILE_RUN, compiler,
      "c-size", NULL, 0);
  comptest_reset (check->comptest);
  if (rc < 0) {
    rc = 0;
  }
  return rc;
}

int
mkc_chk_type (mkc_check_t *check,
    mkc_compiler_t compiler, const char *type)
{
  int             rc;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: type: %s\n", type);

  scopedvar_set_str (check->scopedvar, SV_T_LOCAL, "MKC_TV_TEST_TYPE", type, MKC_VCTXT_TEMP);

  rc = comptest_test (check->comptest, MKC_COMPILE_ONLY, compiler,
      "c-type", NULL, 0);
  comptest_reset (check->comptest);
  return rc;
}

int
mkc_chk_struct_member (mkc_check_t *check,
    mkc_compiler_t compiler,
    const char *structname, const char *membername)
{
  int             rc;

  mkc_log (check->log, MKC_LOG_CHECK,
      "== chk: struct member: %s.%s\n", structname, membername);

  scopedvar_set_str (check->scopedvar, SV_T_LOCAL, "MKC_TV_TEST_STRUCT_NAME", structname, MKC_VCTXT_TEMP);
  scopedvar_set_str (check->scopedvar, SV_T_LOCAL, "MKC_TV_TEST_STRUCT_MEMBER", membername, MKC_VCTXT_TEMP);

  rc = comptest_test (check->comptest, MKC_COMPILE_ONLY, compiler,
      "c-struct-member", NULL, 0);
  comptest_reset (check->comptest);
  return rc;
}

int
mkc_chk_function (mkc_check_t *check, mkc_compiler_t compiler,
    const char *funcname)
{
  int             rc;

  mkc_log (check->log, MKC_LOG_CHECK,
      "== chk: function: %s\n", funcname);

  scopedvar_set_str (check->scopedvar, SV_T_LOCAL, "MKC_TV_TEST_FUNCTION_NAME", funcname, MKC_VCTXT_TEMP);

  rc = comptest_test (check->comptest, MKC_COMPILE_LINK, compiler,
      "c-function", NULL, 0);
  comptest_reset (check->comptest);
  return rc;
}

int
mkc_chk_header (mkc_check_t *check, mkc_compiler_t compiler,
    const char *header, const char * compflags [], const char * ldflags [])
{
  int             rc;
  char            tbuff [MKC_VNAME_MAX];
  char            bc, ec;

  mkc_log (check->log, MKC_LOG_CHECK,
      "== chk: header: %s\n", header);

  bc = '<';
  ec = '>';
  if (check->attr->localheader) {
    bc = '"';
    ec = '"';
  }
  snprintf (tbuff, sizeof (tbuff), "%c%s%c", bc, header, ec);
  scopedvar_set_str (check->scopedvar, SV_T_LOCAL, "MKC_TV_TEST_HEADER", tbuff, MKC_VCTXT_TEMP);

  comptest_set_flags (check->comptest, compflags, ldflags, NULL);
  rc = comptest_test (check->comptest, MKC_COMPILE_LINK, compiler,
      "c-header", NULL, 0);
  comptest_reset (check->comptest);
  return rc;
}

void
mkc_check_get_include_deps (mkc_check_t *check,
    mkc_compiler_t compiler, const char *filename, const char *filepath)
{
  time_t      fts;
#if _have_regex
  char        ** match;
  int         matchcount = 0;
  mkc_list_t  * elist;
  char        * rbuff;
  size_t      fsz = 0;
  time_t      currtm;

  currtm = mstime ();
  fts = 0;
  if (scopedvar_is_defined (check->scopedvar, SV_T_TIMESTAMP, filename)) {
    fts = scopedvar_get_timestamp (check->scopedvar, SV_T_TIMESTAMP, filename);
  }
  if (currtm < fts &&
      scopedvar_is_defined (check->scopedvar, SV_T_DEPENDENCY, filename)) {
    return;
  }

  if (check->rxincludedep == NULL) {
    check->rxincludedep = mkc_regex_init (
        "^# *(include|import) *\"?([^\"<>]+)\"?$",
        MKC_REGEX_MULTILINE, check->mkcerr);
    if (mkc_error_chk_err (check->mkcerr)) {
      return;
    }
  }

  /* note that the lists loaded from the cache are not sorted */
  /* since they are supposedly complete, they should not need to be searched */
  /* and processing should work.  this may need to be re-visited later */

  /* keep the dependency list sorted so that duplicates can be found */
  /* any existing list must be cleared, so set the variable to an empty list */

  elist = mkc_list_init (MKC_LIST_SORTED, NULL, value_str_compare, check->mkcerr);
  scopedvar_set_list (check->scopedvar, SV_T_DEPENDENCY,
      filename, elist, MKC_VCTXT_MKC);
  mkc_list_free (elist);

  mkc_regex_get_reset (check->rxincludedep);

  rbuff = fileop_read_file (filepath, &fsz, check->mkcerr);

  while (true) {
    const char      * dep;

    match = mkc_regex_get (check->rxincludedep, rbuff, &matchcount);
    if (matchcount != 3) {
      mkc_regex_get_free (match);
      break;
    }

    mkc_log (check->log, MKC_LOG_CHECK, "  get-inc-deps: match: %s\n", match [2]);
    dep = match [2];

    scopedvar_append_str_list (check->scopedvar, SV_T_DEPENDENCY,
        filename, dep, MKC_VCTXT_MKC);

    mkc_regex_get_free (match);
  }

  free (rbuff);

  mkc_regex_free (check->rxargcount);
  check->rxargcount = NULL;
#endif

  /* set the timestamp when the file is processed */
  /* convert to ms for comparison to the real time */
  fts = fileop_modtime (filepath);
  fts *= 1000;
  scopedvar_set_timestamp (check->scopedvar, SV_T_TIMESTAMP, filename, fts, MKC_VCTXT_MKC);
}

/* internal routines */

static mkc_err_code_t
mkc_chk_env_var_set (mkc_check_t *check, const char *nm)
{
  char            *tbuff;
  mkc_err_code_t  rc = MKC_OK;

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  *tbuff = '\0';
  env_get (nm, tbuff, MKC_PATH_MAX);
  if (*tbuff) {
    rc = scopedvar_set_str (check->scopedvar, SV_T_INTERNAL, nm, tbuff, MKC_VCTXT_ENV);
  }

  free (tbuff);

  return rc;
}

static int
mkc_chk_package_exec (mkc_check_t *check, const char *pkg)
{
  value_t           *value;
  char              *pkgconfpath;
  char              *tpath;
  const char        *tmp;
  char              tmpname [MKC_VNAME_MAX];
  char              *rbuff;
  size_t            rsz;
  size_t            retsz;
  int               rc;
  mkc_alternate_t   * alt;
  mkc_listidx_t     iteridx;
  mkc_listidx_t     pathidx;
  int               targc = 0;
  const char        * targv [10];

  pkgconfpath = malloc (MKC_PATH_MAX);
  if (pkgconfpath == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  *pkgconfpath = '\0';
  /* if pkgconf is installed, pkg-config is a symlink. */
  /* use pkg-config by preference (pkgconf does not seem to work in macos macports) */
  value = scopedvar_get_value (check->scopedvar, SV_T_INTERNAL, MKC_C_PATH_PKGCONFIG);
  if (value == NULL) {
    value = scopedvar_get_value (check->scopedvar, SV_T_INTERNAL, MKC_C_PATH_PKGCONF);
  }
  if (value != NULL) {
    scopedvar_value_get_str (check->scopedvar, value, pkgconfpath, MKC_PATH_MAX);
  }

  if (! *pkgconfpath) {
    mkc_error_set (check->mkcerr, MKC_ERR_PKGCONF_NOT_FOUND, 0, NULL);
    free (pkgconfpath);
    return MKC_ERR_FAILURE;
  }

  targv [targc++] = pkgconfpath;

  tpath = malloc (MKC_PATH_MAX);
  if (tpath == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  mkc_list_iter_start (check->attr->pathlist, &iteridx);
  while ((pathidx = mkc_list_iter_next (check->attr->pathlist, &iteridx)) != MKC_ITER_FINISH) {
    value_t   *path;

    path = mkc_list_get_by_idx (check->attr->pathlist, pathidx);
    scopedvar_value_get_str (check->scopedvar, path, tpath, MKC_PATH_MAX);
    targv [targc++] = "--with-path";
    targv [targc++] = tpath;
  }

  targv [targc++] = "--exists";
  targv [targc++] = pkg;
  targv [targc++] = NULL;
  if (mkc_error_chk_err (check->mkcerr)) {
    free (pkgconfpath);
    free (tpath);
    return MKC_ERR_FAILURE;
  }

  rsz = MKC_SMALL_BUFF_SZ;
  rbuff = malloc (rsz);
  if (rbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (pkgconfpath);
    free (tpath);
    return MKC_ERR_FAILURE;
  }
  mkc_log_command (check->log, "pkg-exists: cmd: ", targc, targv);

  rc = os_process_pipe (targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);
  mkc_log (check->log, MKC_LOG_CHECK, "pkg exists: %s\n", pkg);
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (rc != MKC_OK) {
    free (pkgconfpath);
    free (rbuff);
    free (tpath);
    return rc;
  }

  targc = 0;
  targv [targc++] = pkgconfpath;

  mkc_list_iter_start (check->attr->pathlist, &iteridx);
  while ((pathidx = mkc_list_iter_next (check->attr->pathlist, &iteridx)) != MKC_ITER_FINISH) {
    value_t   *path;

    path = mkc_list_get_by_idx (check->attr->pathlist, pathidx);
    scopedvar_value_get_str (check->scopedvar, path, tpath, MKC_PATH_MAX);
    targv [targc++] = "--with-path";
    targv [targc++] = tpath;
  }

  targv [targc++] = "--cflags";
  targv [targc++] = pkg;
  targv [targc++] = NULL;
  if (mkc_error_chk_err (check->mkcerr)) {
    free (pkgconfpath);
    free (rbuff);
    free (tpath);
    return MKC_ERR_FAILURE;
  }

  mkc_log_command (check->log, "pkg-cflags: cmd: ", targc, targv);

  alt = check->attr->curralt;
  tmp = pkg;
  if (alt->name != NULL) {
    tmp = alt->name;
  }

  rc = os_process_pipe (targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);
  mkc_log (check->log, MKC_LOG_CHECK, "pkg cflags: %s", rbuff);
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (rc != MKC_OK) {
    free (pkgconfpath);
    free (rbuff);
    free (tpath);
    return rc;
  }

  /* make sure a list exists */
  snprintf (tmpname, sizeof (tmpname), "%s_CFLAGS", tmp);
  str_clean (tmpname, 0);
  scopedvar_append_str_list (check->scopedvar, SV_T_SEARCH, tmpname, NULL, MKC_VCTXT_MKC);

  if (retsz > 0) {
    str_trim (rbuff, retsz);
    scopedvar_set_list_from_str (check->scopedvar, tmpname, rbuff, MKC_VCTXT_MKC);
  }

  targc = 0;
  targv [targc++] = pkgconfpath;

  mkc_list_iter_start (check->attr->pathlist, &iteridx);
  while ((pathidx = mkc_list_iter_next (check->attr->pathlist, &iteridx)) != MKC_ITER_FINISH) {
    value_t   *path;

    path = mkc_list_get_by_idx (check->attr->pathlist, pathidx);
    scopedvar_value_get_str (check->scopedvar, path, tpath, MKC_PATH_MAX);
    targv [targc++] = "--with-path";
    targv [targc++] = tpath;
  }

  targv [targc++] = "--libs";
  targv [targc++] = pkg;
  targv [targc++] = NULL;
  if (mkc_error_chk_err (check->mkcerr)) {
    free (pkgconfpath);
    free (rbuff);
    free (tpath);
    return MKC_ERR_FAILURE;
  }

  mkc_log_command (check->log, "pkg-libs: cmd: ", targc, targv);

  rc = os_process_pipe (targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);
  mkc_log (check->log, MKC_LOG_CHECK, "pkg libs: %s\n", rbuff);
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);

  /* make sure a list exists */
  snprintf (tmpname, sizeof (tmpname), "%s_LIBS", tmp);
  str_clean (tmpname, 0);
  scopedvar_append_str_list (check->scopedvar, SV_T_SEARCH, tmpname, NULL, MKC_VCTXT_MKC);

  if (retsz > 0) {
    str_trim (rbuff, retsz);
    scopedvar_set_list_from_str (check->scopedvar, tmpname, rbuff, MKC_VCTXT_MKC);
  }

  free (pkgconfpath);
  free (rbuff);
  free (tpath);
  return rc;
}

