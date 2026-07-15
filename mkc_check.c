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

#include "mkc_check.h"
#include "mkc_compiler.h"
#include "mkc_const.h"
#include "mkc_def.h"
#include "mkc_env.h"
#include "mkc_error.h"
#include "mkc_fileop.h"
#include "mkc_log.h"
#include "mkc_pvar.h"
#include "mkc_os_process.h"
#include "mkc_path.h"
#include "mkc_regex.h"
#include "mkc_string.h"

#define MKC_PKG_TRACE 0

typedef enum {
  MKC_CHK_TEST_COMPILE_ONLY,
  MKC_CHK_TEST_COMPILE_LINK,
  MKC_CHK_TEST_COMPILE_RUN,
} mkc_check_test_t;

typedef struct mkc_check_t {
  mkc_profile_t     * profiles;
  mkc_pvar_t        * pvar;
  mkc_error_t       * mkcerr;
  mkc_log_t         * log;
  mkc_attribute_t   * attr;
  const   char      ** targv;
  char              * pkgname;
  mkc_regex_t       * rxargcount;
  mkc_regex_t       * rxcomma;
  mkc_regex_t       * rxincludedep;
  mkc_profidx_t     pidx_internal;
  mkc_profidx_t     pidx_temp;
  mkc_profidx_t     pidx_dflt_comp;
  int               targc;
  int               targvallocsz;
} mkc_check_t;

static char const * const MKC_C_TEST_HDR_LIST = "MKC_TV_TEST_HEADER_LIST";

static void mkc_check_file_sub_copy (mkc_check_t *check, char *tbuff, size_t sz, const char *fname, const char *origsfx, const char *sfx);
static void mkc_check_log_command (mkc_check_t *check, const char *tag);
static mkc_err_code_t mkc_chk_env_var_set (mkc_check_t *check, const char *nm);
static void mkc_check_append_arg (mkc_check_t *check, const char *arg);
static const char * mkc_check_get_compstr (mkc_check_t *check, mkc_compiler_t compiler, char *buff, size_t sz);
static int mkc_chk_package_exec (mkc_check_t *check, const char *pkg);
static void mkc_chk_create_header_var (mkc_check_t *check);
static void mkc_check_append_list_arg (mkc_check_t *check, mkc_list_t *list);

typedef int (*test_func_t)(mkc_check_t *check, mkc_compiler_t compiler, const char *fname, const char *flags [], char *rbuff, size_t rsz);

static int mkc_do_test (mkc_check_test_t type, mkc_check_t *check, mkc_compiler_t compiler, const char *fname, const char * flags[], char *rbuff, size_t rsz);
static int mkc_compile_only (mkc_check_t *check, mkc_compiler_t compiler, const char *fname, const char *flags[], char *rbuff, size_t rsz);
static int mkc_compile_link (mkc_check_t *check, mkc_compiler_t compiler, const char *fname, const char *flags[], char *rbuff, size_t rsz);
static int mkc_compile_run (mkc_check_t *check, mkc_compiler_t compiler, const char *fname, const char *flags[], char *rbuff, size_t rsz);

MKC_NODISCARD
mkc_check_t *
mkc_check_init (mkc_profile_t *profiles, mkc_pvar_t *pvar,
    mkc_attribute_t *attr, mkc_log_t *log,
    mkc_profidx_t pidx, mkc_error_t *mkcerr)
{
  mkc_check_t   *check;
  mkc_profidx_t tpidx;

  check = malloc (sizeof (mkc_check_t));
  check->profiles = profiles;
  check->pvar = pvar;
  check->attr = attr;
  check->mkcerr = mkcerr;
  check->log = log;
  check->pkgname = NULL;
  check->rxargcount = NULL;
  check->rxcomma = NULL;
  check->rxincludedep = NULL;

  tpidx = mkc_profile_find (check->profiles,
      MKC_C_PROF_INTERNAL_NAME, MKC_COMPILER_GENERAL);
  check->pidx_internal = tpidx;
  tpidx = mkc_profile_find (check->profiles,
      MKC_C_PROF_TEMP_NAME, MKC_COMPILER_GENERAL);
  check->pidx_temp = tpidx;
  check->pidx_dflt_comp = pidx;

  check->targv = NULL;
  check->targc = 0;
  check->targvallocsz = 10;
  check->targv = malloc (check->targvallocsz * sizeof (const char *));
  if (check->targv == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
  }
  mkc_chk_reset (check);

  return check;
}

void
mkc_check_free (mkc_check_t *check)
{
  if (check == NULL) {
    return;
  }

  datafree (check->targv);
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
  rc = mkc_compile_only (check, compiler, "int-main", NULL, NULL, 0);
  mkc_chk_reset (check);
  return rc;
}

void
mkc_chk_reset (mkc_check_t *check)
{
  if (check == NULL) {
    return;
  }

  check->targv [0] = NULL;
  check->targc = 0;
}

void
mkc_chk_append_comp_flag (mkc_check_t *check, const char *flag)
{
  mkc_value_t     tvalue;
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
  mkc_alternate_t  * alt;

  if (check == NULL || flag == NULL) {
    return;
  }

  tvalue.sval = (char *) flag;
  tvalue.vtype = MKC_VT_STRING;
  alt = check->attr->curralt;
  mkc_list_set (alt->compflags, &tvalue, sizeof (mkc_value_t), &loc);
}

void
mkc_chk_append_link_flag (mkc_check_t *check, const char *flag)
{
  mkc_value_t     tvalue;
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
  mkc_alternate_t  * alt;

  if (check == NULL || flag == NULL) {
    return;
  }

  tvalue.sval = (char *) flag;
  tvalue.vtype = MKC_VT_STRING;
  alt = check->attr->curralt;
  mkc_list_set (alt->linkflags, &tvalue, sizeof (mkc_value_t), &loc);
}

int
mkc_chk_header_modern (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: header-modern\n");
  rc = mkc_compile_only (check, compiler, "int-header-modern", NULL, NULL, 0);
  mkc_chk_reset (check);
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
    mkc_chk_reset (check);
    return MKC_SYS_UNKNOWN;
  }
  mkc_path_build (MKC_PATH_MKC_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  rc = mkc_compile_run (check, compiler, "int-system", flags, NULL, 0);
  mkc_chk_reset (check);
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
    mkc_chk_reset (check);
    return MKC_SYS_ID_NOTSET;
  }
  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: system-id\n");
  mkc_path_build (MKC_PATH_MKC_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  rc = mkc_compile_run (check, compiler, "int-sysid", flags, NULL, 0);
  mkc_chk_reset (check);
  free (inc);
  return rc;
}

int
mkc_chk_variadic_macro (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: variadic-macro\n");
  rc = mkc_compile_only (check, compiler, "int-variadic-macro", NULL, NULL, 0);
  mkc_chk_reset (check);
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
    mkc_chk_reset (check);
    return 0;
  }
  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: lib-location\n");
  mkc_path_build (MKC_PATH_MKC_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  rc = mkc_compile_run (check, compiler, "int-libloc", flags, NULL, 0);
  mkc_chk_reset (check);
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
    mkc_chk_reset (check);
    return 0;
  }
  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: compiler-id\n");
  mkc_path_build (MKC_PATH_MKC_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  rc = mkc_compile_run (check, compiler, "int-compid", flags, NULL, 0);
  mkc_chk_reset (check);
  free (inc);
  return rc;
}

int
mkc_chk_arg_count (mkc_check_t *check, mkc_compiler_t compiler,
    const char *funcname)
{
  int             rc = 0;
  mkc_profidx_t   opidx;
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
  opidx = mkc_profile_get_active (check->profiles);

  mkc_pvar_profile_select_idx (check->pvar, check->pidx_temp);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_FUNCTION_NAME", funcname, MKC_VCTXT_TEMP);
  mkc_pvar_profile_select_idx (check->pvar, opidx);

  rbuff = malloc (rsz);
  if (rbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    mkc_chk_reset (check);
    return MKC_OK;
  }

  flags [fcount++] = "-E";
  flags [fcount++] = NULL;
  rc = mkc_do_test (MKC_CHK_TEST_COMPILE_ONLY,
      check, compiler, "c-argcount", flags, rbuff, rsz);

  /*  int mkdir (const char *__path, __mode_t __mode) */
  /*      ;   */

#if _have_regex
  if (check->rxcomma == NULL) {
    check->rxcomma = mkc_regex_init ("(,)", MKC_REGEX_NONE, check->mkcerr);
    if (mkc_error_chk_err (check->mkcerr)) {
      free (rbuff);
      mkc_chk_reset (check);
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
    mkc_chk_reset (check);
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

  mkc_chk_reset (check);
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
    mkc_chk_reset (check);
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
  rc = mkc_compile_only (check, compiler, "c-main", NULL, rbuff, rsz);
  if (rc == 0) {
    /* clang does not return an error code on a unknown warning */
    if (strstr (rbuff, "warning") != NULL) {
      rc = 1;
    }
  }
  mkc_chk_reset (check);

  free (rbuff);
  return rc;
}

int
mkc_chk_const (mkc_check_t *check,
    mkc_compiler_t compiler, const char *consttxt)
{
  int             rc;
  mkc_profidx_t   opidx;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: constant: %s\n", consttxt);
  opidx = mkc_profile_get_active (check->profiles);

  mkc_pvar_profile_select_idx (check->pvar, check->pidx_temp);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_CONSTANT", consttxt, MKC_VCTXT_TEMP);

  mkc_pvar_profile_select_idx (check->pvar, opidx);

  rc = mkc_do_test (MKC_CHK_TEST_COMPILE_ONLY,
      check, compiler, "c-const", NULL, NULL, 0);
  mkc_chk_reset (check);
  return rc;
}

int
mkc_chk_define (mkc_check_t *check,
    mkc_compiler_t compiler, const char *def)
{
  int             rc;
  mkc_profidx_t   opidx;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: define: %s\n", def);
  opidx = mkc_profile_get_active (check->profiles);

  mkc_pvar_profile_select_idx (check->pvar, check->pidx_temp);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_DEFINE", def, MKC_VCTXT_TEMP);

  mkc_pvar_profile_select_idx (check->pvar, opidx);

  rc = mkc_do_test (MKC_CHK_TEST_COMPILE_ONLY,
      check, compiler, "c-define", NULL, NULL, 0);
  mkc_chk_reset (check);
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

  mkc_chk_reset (check);
  return rc;
}

int
mkc_chk_link_flag (mkc_check_t *check,
    mkc_compiler_t compiler,
    const char *flag)
{
  int               rc;
  char              *rbuff;

  rbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (rbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    mkc_chk_reset (check);
    return MKC_ERR_FAILURE;
  }

  mkc_chk_append_link_flag (check, flag);
  mkc_log (check->log, MKC_LOG_CHECK, "== chk: link-flag: %s\n", flag);
  rc = mkc_compile_link (check, compiler, "c-main", NULL, rbuff, MKC_PATH_MAX);
  if (rc == 0) {
    /* clang does not return an error code on a unknown warning */
    if (strstr (rbuff, "warning") != NULL) {
      rc = 1;
    }
  }
  mkc_chk_reset (check);
  free (rbuff);
  return rc;
}

int
mkc_chk_size (mkc_check_t *check,
    mkc_compiler_t compiler,
    const char *type)
{
  int             rc;
  mkc_profidx_t   opidx;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: size: %s\n", type);
  opidx = mkc_profile_get_active (check->profiles);

  mkc_pvar_profile_select_idx (check->pvar, check->pidx_temp);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_SIZE", type, MKC_VCTXT_TEMP);

  mkc_pvar_profile_select_idx (check->pvar, opidx);

  rc = mkc_do_test (MKC_CHK_TEST_COMPILE_RUN,
      check, compiler, "c-size", NULL, NULL, 0);
  if (rc < 0) {
    rc = 0;
  }
  mkc_chk_reset (check);
  return rc;
}

int
mkc_chk_type (mkc_check_t *check,
    mkc_compiler_t compiler,
    const char *type)
{
  int             rc;
  mkc_profidx_t   opidx;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: type: %s\n", type);
  opidx = mkc_profile_get_active (check->profiles);

  mkc_pvar_profile_select_idx (check->pvar, check->pidx_temp);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_TYPE", type, MKC_VCTXT_TEMP);

  mkc_pvar_profile_select_idx (check->pvar, opidx);

  rc = mkc_do_test (MKC_CHK_TEST_COMPILE_ONLY,
      check, compiler, "c-type", NULL, NULL, 0);
  mkc_chk_reset (check);
  return rc;
}

int
mkc_chk_struct_member (mkc_check_t *check,
    mkc_compiler_t compiler,
    const char *structname, const char *membername)
{
  int             rc;
  mkc_profidx_t   opidx;

  mkc_log (check->log, MKC_LOG_CHECK,
      "== chk: struct member: %s.%s\n", structname, membername);
  opidx = mkc_profile_get_active (check->profiles);

  mkc_pvar_profile_select_idx (check->pvar, check->pidx_temp);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_STRUCT_NAME", structname, MKC_VCTXT_TEMP);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_STRUCT_MEMBER", membername, MKC_VCTXT_TEMP);

  mkc_pvar_profile_select_idx (check->pvar, opidx);

  rc = mkc_do_test (MKC_CHK_TEST_COMPILE_ONLY,
      check, compiler, "c-struct-member", NULL, NULL, 0);
fprintf (stderr, "member: %s.%s %d\n", structname, membername, rc);
  mkc_chk_reset (check);
  return rc;
}

int
mkc_chk_function (mkc_check_t *check, mkc_compiler_t compiler,
    const char *funcname)
{
  int             rc;
  mkc_profidx_t   opidx;

  mkc_log (check->log, MKC_LOG_CHECK,
      "== chk: function: %s\n", funcname);

  opidx = mkc_profile_get_active (check->profiles);

  mkc_pvar_profile_select_idx (check->pvar, check->pidx_temp);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_FUNCTION_NAME", funcname, MKC_VCTXT_TEMP);

  mkc_pvar_profile_select_idx (check->pvar, opidx);

  rc = mkc_do_test (MKC_CHK_TEST_COMPILE_LINK,
      check, compiler, "c-function", NULL, NULL, 0);
  mkc_chk_reset (check);
  return rc;
}

int
mkc_chk_header (mkc_check_t *check, mkc_compiler_t compiler,
    const char *header, const char * flags [])
{
  int             rc;
  mkc_profidx_t   opidx;
  char            tbuff [MKC_VNAME_MAX];
  char            bc, ec;

  mkc_log (check->log, MKC_LOG_CHECK,
      "== chk: header: %s\n", header);

  opidx = mkc_profile_get_active (check->profiles);

  mkc_pvar_profile_select_idx (check->pvar, check->pidx_temp);

  bc = '<';
  ec = '>';
  if (check->attr->localheader) {
    bc = '"';
    ec = '"';
  }
  snprintf (tbuff, sizeof (tbuff), "%c%s%c", bc, header, ec);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_HEADER", tbuff, MKC_VCTXT_TEMP);

  mkc_pvar_profile_select_idx (check->pvar, opidx);

  rc = mkc_do_test (MKC_CHK_TEST_COMPILE_LINK,
      check, compiler, "c-header", flags, NULL, 0);
  mkc_chk_reset (check);
  return rc;
}

void
mkc_check_get_include_deps (mkc_check_t *check,
    mkc_compiler_t compiler, const char *rbuff, mkc_list_t *deplist)
{
#if _have_regex
  char    **match;
  int     matchcount = 0;
#endif

  mkc_log (check->log, MKC_LOG_CHECK, "== get-include-deps\n");

#if _have_regex
  if (check->rxincludedep == NULL) {
    check->rxincludedep = mkc_regex_init (
        "^# *(include|import) *\"?([^\"<>]+)\"?$",
        MKC_REGEX_MULTILINE, check->mkcerr);
    if (mkc_error_chk_err (check->mkcerr)) {
      mkc_chk_reset (check);
      return;
    }
  }

  mkc_regex_get_reset (check->rxincludedep);

  while (true) {
    char            *tp;
    mkc_listidx_t   loc;

    match = mkc_regex_get (check->rxincludedep, rbuff, &matchcount);
    mkc_log (check->log, MKC_LOG_CHECK, "  get-inc-deps: matches: %d\n", matchcount);
    if (matchcount != 3) {
      mkc_regex_get_free (match);
      break;
    }

    tp = strdup (match [2]);
    mkc_list_set (deplist, &tp, sizeof (char *), &loc);

    mkc_regex_get_free (match);
  }

  mkc_regex_free (check->rxargcount);
  check->rxargcount = NULL;
#endif

  mkc_chk_reset (check);
}

/* internal routines */

static void
mkc_check_file_sub_copy (mkc_check_t *check,
    char *tbuff, size_t sz,
    const char *fname, const char *origsfx, const char *sfx)
{
  char    tfn [MKC_VNAME_MAX];
  char    *fbuff;
  char    *data;
  char    *ndata;
  FILE    *fh;
  size_t  fsz;

  fbuff = malloc (MKC_PATH_MAX);
  if (fbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  snprintf (tfn, sizeof (tfn), "%s%s", fname, origsfx);
  mkc_path_build (MKC_PATH_MKC_TEMPLATES, fbuff, MKC_PATH_MAX, tfn, check->mkcerr);
  mkc_log (check->log, MKC_LOG_CHECK, "filename: %s\n", fbuff);
  data = mkc_read_file (fbuff, &fsz, check->mkcerr);
  if (mkc_error_chk_err (check->mkcerr)) {
    free (fbuff);
    return;
  }
  ndata = mkc_pvar_substitute (check->pvar, data, MKC_PV_NO_ESCAPE, 0);
  mkc_log (check->log, MKC_LOG_CHECK, "--- code:\n");
  mkc_log (check->log, MKC_LOG_CHECK, "%s\n", ndata);
  mkc_log (check->log, MKC_LOG_CHECK, "---\n");
  free (data);

  snprintf (tfn, sizeof (tfn), "%s%s", fname, sfx);
  mkc_path_build (MKC_PATH_MKC_TMP, tbuff, sz, tfn, check->mkcerr);

  fh = mkc_fopen (tbuff, "wb");
  if (fh == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, tbuff);
  } else {
    fwrite (ndata, strlen (ndata), 1, fh);
    fclose (fh);
  }
  free (ndata);
  free (fbuff);
}

static void
mkc_check_log_command (mkc_check_t *check, const char *tag)
{
  int   targc;

  targc = 0;
  mkc_log (check->log, MKC_LOG_CHECK, "%s: cmd: ", tag);
  while (check->targv [targc] != NULL) {
    mkc_log (check->log, MKC_LOG_CHECK, "%s ", check->targv [targc]);
    ++targc;
  }
  mkc_log (check->log, MKC_LOG_CHECK, "\n");
  if (targc == 0) {
    mkc_log (check->log, MKC_LOG_CHECK, "invalid count %d\n", targc);
  }
  if (targc + 1 != check->targc) {
    mkc_log (check->log, MKC_LOG_CHECK, "mismatch count %d %d\n", targc + 1, check->targc);
  }
  mkc_log_flush (check->log);
}

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
  mkc_env_get (nm, tbuff, MKC_PATH_MAX);
  if (*tbuff) {
    rc = mkc_pvar_set_str (check->pvar, nm, tbuff, MKC_VCTXT_ENV);
  }

  free (tbuff);

  return rc;
}

static void
mkc_check_append_arg (mkc_check_t *check, const char *arg)
{
  if (check == NULL) {
    return;
  }

  if (check->targc >= check->targvallocsz) {
    check->targvallocsz += 10;
    check->targv = realloc (check->targv,
        check->targvallocsz * sizeof (const char *));
    if (check->targv == NULL) {
      mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return;
    }
  }

  check->targv [check->targc] = arg;
  check->targc += 1;
}

static const char *
mkc_check_get_compstr (mkc_check_t *check, mkc_compiler_t compiler,
    char *buff, size_t sz)
{
  const char    *envstr;
  mkc_value_t   *value;

  envstr = mkc_compiler_get_env_name (compiler);
  value = mkc_pvar_get_by_profidx (check->pvar, envstr, check->pidx_internal);
  mkc_pvar_value_get_str (check->pvar, value, buff, sz);
  return buff;
}

static int
mkc_chk_package_exec (mkc_check_t *check, const char *pkg)
{
  mkc_value_t       *value;
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

  pkgconfpath = malloc (MKC_PATH_MAX);
  if (pkgconfpath == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  *pkgconfpath = '\0';
  /* if pkgconf is installed, pkg-config is a symlink. */
  /* use pkg-config by preference (pkgconf does not seem to work in macos macports) */
  value = mkc_pvar_get_by_profidx (check->pvar, MKC_C_PATH_PKGCONFIG, check->pidx_internal);
  if (value == NULL) {
    value = mkc_pvar_get_by_profidx (check->pvar, MKC_C_PATH_PKGCONF, check->pidx_internal);
  }
  if (value != NULL) {
    mkc_pvar_value_get_str (check->pvar, value, pkgconfpath, MKC_PATH_MAX);
  }

  if (! *pkgconfpath) {
    mkc_error_set (check->mkcerr, MKC_ERR_PKGCONF_NOT_FOUND, 0, NULL);
    free (pkgconfpath);
    return MKC_ERR_FAILURE;
  }

  check->targc = 0;
  mkc_check_append_arg (check, pkgconfpath);

  tpath = malloc (MKC_PATH_MAX);
  if (tpath == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  mkc_list_iter_start (check->attr->pathlist, &iteridx);
  while ((pathidx = mkc_list_iter_next (check->attr->pathlist, &iteridx)) != MKC_ITER_FINISH) {
    mkc_value_t   *path;

    path = mkc_list_get_by_idx (check->attr->pathlist, pathidx);
    mkc_pvar_value_get_str (check->pvar, path, tpath, MKC_PATH_MAX);
    mkc_check_append_arg (check, "--with-path");
    mkc_check_append_arg (check, tpath);
  }

  mkc_check_append_arg (check, "--exists");
  mkc_check_append_arg (check, pkg);
  mkc_check_append_arg (check, NULL);
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
  mkc_check_log_command (check, "pkg-exists");

  rc = mkc_os_process_pipe (check->targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);
  mkc_log (check->log, MKC_LOG_CHECK, "pkg exists: %s\n", pkg);
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (rc != MKC_OK) {
    free (pkgconfpath);
    free (rbuff);
    free (tpath);
    return rc;
  }

  check->targc = 0;
  mkc_check_append_arg (check, pkgconfpath);

  mkc_list_iter_start (check->attr->pathlist, &iteridx);
  while ((pathidx = mkc_list_iter_next (check->attr->pathlist, &iteridx)) != MKC_ITER_FINISH) {
    mkc_value_t   *path;

    path = mkc_list_get_by_idx (check->attr->pathlist, pathidx);
    mkc_pvar_value_get_str (check->pvar, path, tpath, MKC_PATH_MAX);
    mkc_check_append_arg (check, "--with-path");
    mkc_check_append_arg (check, tpath);
  }

  mkc_check_append_arg (check, "--cflags");
  mkc_check_append_arg (check, pkg);
  mkc_check_append_arg (check, NULL);
  if (mkc_error_chk_err (check->mkcerr)) {
    free (pkgconfpath);
    free (rbuff);
    free (tpath);
    mkc_chk_reset (check);
    return MKC_ERR_FAILURE;
  }

  mkc_check_log_command (check, "pkg-cflags");

  alt = check->attr->curralt;
  tmp = pkg;
  if (alt->name != NULL) {
    tmp = alt->name;
  }

  rc = mkc_os_process_pipe (check->targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);
  mkc_log (check->log, MKC_LOG_CHECK, "pkg cflags: %s", rbuff);
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (rc != MKC_OK) {
    free (pkgconfpath);
    free (rbuff);
    free (tpath);
    mkc_chk_reset (check);
    return rc;
  }

  /* make sure a list exists */
  snprintf (tmpname, sizeof (tmpname), "%s_CFLAGS", tmp);
  mkc_strclean (tmpname, 0);
  mkc_pvar_append_str_list (check->pvar, tmpname, NULL, MKC_VCTXT_MKC);

  if (retsz > 0) {
    mkc_strtrim (rbuff, retsz);
    mkc_pvar_set_list_from_str (check->pvar, tmpname, rbuff, MKC_VCTXT_MKC);
  }

  mkc_chk_reset (check);
  check->targc = 0;
  mkc_check_append_arg (check, pkgconfpath);

  mkc_list_iter_start (check->attr->pathlist, &iteridx);
  while ((pathidx = mkc_list_iter_next (check->attr->pathlist, &iteridx)) != MKC_ITER_FINISH) {
    mkc_value_t   *path;

    path = mkc_list_get_by_idx (check->attr->pathlist, pathidx);
    mkc_pvar_value_get_str (check->pvar, path, tpath, MKC_PATH_MAX);
    mkc_check_append_arg (check, "--with-path");
    mkc_check_append_arg (check, tpath);
  }

  mkc_check_append_arg (check, "--libs");
  mkc_check_append_arg (check, pkg);
  mkc_check_append_arg (check, NULL);
  if (mkc_error_chk_err (check->mkcerr)) {
    free (pkgconfpath);
    free (rbuff);
    free (tpath);
    mkc_chk_reset (check);
    return MKC_ERR_FAILURE;
  }

  mkc_check_log_command (check, "pkg-libs");

  rc = mkc_os_process_pipe (check->targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);
  mkc_log (check->log, MKC_LOG_CHECK, "pkg libs: %s\n", rbuff);
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);

  /* make sure a list exists */
  snprintf (tmpname, sizeof (tmpname), "%s_LIBS", tmp);
  mkc_strclean (tmpname, 0);
  mkc_pvar_append_str_list (check->pvar, tmpname, NULL, MKC_VCTXT_MKC);

  if (retsz > 0) {
    mkc_strtrim (rbuff, retsz);
    mkc_pvar_set_list_from_str (check->pvar, tmpname, rbuff, MKC_VCTXT_MKC);
  }

  mkc_chk_reset (check);
  free (pkgconfpath);
  free (rbuff);
  free (tpath);
  return rc;
}

static void
mkc_chk_create_header_var (mkc_check_t *check)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;
  char            * hdrtxt = NULL;
  char            * tmp = NULL;
  size_t          hdrtxtlen = 1;
  mkc_profidx_t   pidx;
  mkc_alternate_t  * alt;


  alt = check->attr->curralt;
  mkc_list_iter_start (alt->hdrlist, &iteridx);
  while ((lidx = mkc_list_iter_next (alt->hdrlist, &iteridx)) != MKC_ITER_FINISH) {
    char        tbuff [MKC_PATH_MAX];
    mkc_value_t *lvalue;
    size_t      tlen;

    if (mkc_error_chk_err (check->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (alt->hdrlist, lidx);
    if (check->attr->headertype == MKC_HEADER_MODERN) {
      snprintf (tbuff, sizeof (tbuff),
          "#if __has_include (<%s>)\n"
          "# include <%s>\n"
          "#endif\n", lvalue->sval, lvalue->sval);
    } else {
      snprintf (tbuff, sizeof (tbuff), "#include <%s>\n", lvalue->sval);
    }
    tlen = strlen (tbuff);
    hdrtxtlen += tlen;
    hdrtxt = realloc (hdrtxt, hdrtxtlen);
    stpecpy (hdrtxt + hdrtxtlen - tlen - 1, hdrtxt + hdrtxtlen, tbuff);
  }

  tmp = hdrtxt;
  if (hdrtxt == NULL) {
    tmp = "";
  }
  pidx = mkc_profile_get_active (check->profiles);
  mkc_pvar_profile_select_idx (check->pvar, check->pidx_temp);
  mkc_pvar_set_str (check->pvar, MKC_C_TEST_HDR_LIST, tmp, MKC_VCTXT_TEMP);
  mkc_pvar_profile_select_idx (check->pvar, pidx);

  free (hdrtxt);
}

static void
mkc_check_append_list_arg (mkc_check_t *check, mkc_list_t *list)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;

  if (check == NULL || list == NULL) {
    return;
  }

  mkc_list_iter_start (list, &iteridx);
  while ((lidx = mkc_list_iter_next (list, &iteridx)) != MKC_ITER_FINISH) {
    mkc_value_t   *lvalue;

    if (mkc_error_chk_err (check->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (list, lidx);
    mkc_check_append_arg (check, lvalue->sval);
  }
}

static int
mkc_do_test (mkc_check_test_t ctype,
    mkc_check_t *check, mkc_compiler_t compiler,
    const char *fname, const char * flags [], char *rbuff, size_t rsz)
{
  int             rc = MKC_ERR_FAILURE;
  int             altsz;
  mkc_alternate_t * oldcurr;
  mkc_list_t      * alternates;
  mkc_alternate_t * alt;
  mkc_listidx_t   iteridx;
  mkc_listidx_t   aidx;
  test_func_t     func = NULL;
  bool            first = true;

  alternates = check->attr->alternates;
  altsz = mkc_list_size (alternates);

  oldcurr = check->attr->curralt;

  switch (ctype) {
    case MKC_CHK_TEST_COMPILE_ONLY: {
      func = mkc_compile_only;
      break;
    }
    case MKC_CHK_TEST_COMPILE_LINK: {
      func = mkc_compile_link;
      break;
    }
    case MKC_CHK_TEST_COMPILE_RUN: {
      func = mkc_compile_run;
      break;
    }
  }

  mkc_list_iter_start (alternates, &iteridx);
  while ((aidx = mkc_list_iter_next (alternates, &iteridx)) != MKC_ITER_FINISH) {
    alt = mkc_list_get_by_idx (alternates, aidx);

    if (first && altsz > 1) {
      /* if any alternates are specified, only test the alternates, */
      /* not the default */
      first = false;
      continue;
    }

    check->attr->curralt = alt;
    mkc_chk_create_header_var (check);
    rc = (*func) (check, compiler, fname, flags, rbuff, rsz);

    /* check doesn't really have the knowledge as to how */
    /* the return code should be processed */
    /* assume for the moment that a return code > 0 */
    /* is a valid test */
    /* this may need to be changed in the future */
    if (rc >= 0 && alt->name != NULL) {
      mkc_pvar_set_integer (check->pvar, alt->name,
          rc >= 0 ? true : false, MKC_VCTXT_CHECK);
    }

    if (rc >= 0) {
      /* the first alternate that succeeds stops the test */
      break;
    }
  }

  check->attr->curralt = oldcurr;
  return rc;
}

static int
mkc_compile_only (mkc_check_t *check, mkc_compiler_t compiler,
    const char *fname, const char *flags [], char *rbuff, size_t rsz)
{
  int             rc;
  char            * tbuff;
  char            * compstr;
  char            * outfile;
  size_t          retsz;
  const char      * sfx = NULL;
  bool            rallocated = false;
  bool            cpreprocess = false;
  mkc_alternate_t * alt;

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }
  compstr = malloc (MKC_PATH_MAX);
  if (compstr == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (tbuff);
    return MKC_ERR_FAILURE;
  }
  outfile = malloc (MKC_PATH_MAX);
  if (outfile == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (tbuff);
    free (compstr);
    return MKC_ERR_FAILURE;
  }

  if (rbuff == NULL) {
    rsz = MKC_SMALL_BUFF_SZ;
    rbuff = malloc (rsz);
    if (rbuff == NULL) {
      mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      free (tbuff);
      free (compstr);
      free (outfile);
      return MKC_ERR_FAILURE;
    }
    *rbuff = '\0';
    rallocated = true;
  }

  mkc_check_get_compstr (check, compiler, compstr, MKC_PATH_MAX);
  sfx = mkc_compiler_get_suffix (compiler);
// ### will need to be fixed, the original suffix may change
  mkc_check_file_sub_copy (check, tbuff, MKC_PATH_MAX, fname, ".c", sfx);

  check->targc = 0;
  mkc_check_append_arg (check, compstr);
  if (flags != NULL) {
    const char  *p;
    int         count = 0;

    while ((p = flags [count++]) != NULL) {
      if (strcmp (p, "-E") == 0) {
        cpreprocess = true;
      }
      mkc_check_append_arg (check, p);
    }
  }

  alt = check->attr->curralt;
  mkc_check_append_list_arg (check, alt->compflags);

  if (! cpreprocess) {
    mkc_check_append_arg (check, "-c");
    mkc_check_append_arg (check, "-o");
    mkc_path_build (MKC_PATH_MKC_TMP, outfile, MKC_PATH_MAX, "mkctest.o", check->mkcerr);
    mkc_check_append_arg (check, outfile);
  }
  mkc_check_append_arg (check, tbuff);
  mkc_check_append_list_arg (check, alt->linkflags);
  mkc_check_append_arg (check, NULL);
  if (mkc_error_chk_err (check->mkcerr)) {
    free (tbuff);
    free (compstr);
    free (outfile);
    if (rallocated) {
      free (rbuff);
    }
    return MKC_ERR_FAILURE;
  }

  mkc_check_log_command (check, "comp-only");

  rc = mkc_os_process_pipe (check->targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);

  if (retsz > 0) {
    mkc_log (check->log, MKC_LOG_CHECK, "--- compile log (%zd)\n", retsz);
    if (retsz < 2000) {
      mkc_log (check->log, MKC_LOG_CHECK, "%s", rbuff);
    } else {
      mkc_log (check->log, MKC_LOG_CHECK_VERBOSE, "%s\n", rbuff);
    }
    mkc_log (check->log, MKC_LOG_CHECK, "---\n");

    if (check->attr->printerrors) {
      fprintf (stderr, "%s", rbuff);
    }
  }
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);

  /* never want the return code to overlap with various enums */
  if (rc > 0) {
    rc = - rc;
  }

  free (tbuff);
  free (compstr);
  free (outfile);
  if (rallocated) {
    free (rbuff);
  }
  return rc;
}

static int
mkc_compile_link (mkc_check_t *check, mkc_compiler_t compiler,
    const char *fname, const char *flags [],
    char *rbuff, size_t rsz)
{
  int               rc;
  size_t            retsz;
  bool              rallocated = false;
  char              *compstr;
  char              *outfile;
  char              *objfile;
  mkc_alternate_t  * alt;


  if (rbuff == NULL) {
    rsz = MKC_SMALL_BUFF_SZ;
    rbuff = malloc (rsz);
    if (rbuff == NULL) {
      mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return MKC_ERR_FAILURE;
    }
    *rbuff = '\0';
    rallocated = true;
  }

  rc = mkc_compile_only (check, compiler, fname, flags, rbuff, rsz);
  if (rc > 0) {
    rc = - rc;
  }
  if (rc != 0) {
    if (rallocated) {
      free (rbuff);
    }
    return rc;
  }

  compstr = malloc (MKC_PATH_MAX);
  if (compstr == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }
  objfile = malloc (MKC_PATH_MAX);
  if (objfile == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (compstr);
    return MKC_ERR_FAILURE;
  }
  outfile = malloc (MKC_PATH_MAX);
  if (outfile == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (objfile);
    free (compstr);
    return MKC_ERR_FAILURE;
  }

  mkc_check_get_compstr (check, compiler, compstr, MKC_PATH_MAX);

  check->targc = 0;
  mkc_check_append_arg (check, compstr);
  mkc_check_append_arg (check, "-o");
  mkc_path_build (MKC_PATH_MKC_TMP, outfile, MKC_PATH_MAX, "mkctest.exe", check->mkcerr);
  mkc_check_append_arg (check, outfile);
  mkc_path_build (MKC_PATH_MKC_TMP, objfile, MKC_PATH_MAX, "mkctest.o", check->mkcerr);
  mkc_check_append_arg (check, objfile);

  alt = check->attr->curralt;
  mkc_check_append_list_arg (check, alt->linkflags);

  mkc_check_append_arg (check, NULL);
  if (mkc_error_chk_err (check->mkcerr)) {
    if (rallocated) {
      free (rbuff);
    }
    return MKC_ERR_FAILURE;
  }

  mkc_check_log_command (check, "link");

  rc = mkc_os_process_pipe (check->targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);

  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (retsz > 0) {
    mkc_log (check->log, MKC_LOG_CHECK, "--- link log\n");
    mkc_log (check->log, MKC_LOG_CHECK, "%s\n", rbuff);
    mkc_log (check->log, MKC_LOG_CHECK, "---\n");
  }

  /* never want the return code to overlap with various enums */
  if (rc > 0) {
    rc = - rc;
  }

  free (objfile);
  free (outfile);
  free (compstr);
  if (rallocated) {
    free (rbuff);
  }

  return rc;
}

static int
mkc_compile_run (mkc_check_t *check, mkc_compiler_t compiler,
    const char *fname, const char *flags [],
    char *rbuff, size_t rsz)
{
  int         rc;
  bool        rallocated = false;
  size_t      retsz;
  char        *exefile;

  rc = mkc_compile_link (check, compiler, fname, flags, NULL, 0);

  if (rc != 0) {
    return rc;
  }

  exefile = malloc (MKC_PATH_MAX);
  if (exefile == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  check->targc = 0;
  mkc_path_build (MKC_PATH_MKC_TMP, exefile, MKC_PATH_MAX, "mkctest.exe", check->mkcerr);
  mkc_check_append_arg (check, exefile);
  mkc_check_append_arg (check, NULL);
  if (mkc_error_chk_err (check->mkcerr)) {
    free (exefile);
    return MKC_ERR_FAILURE;
  }

  mkc_check_log_command (check, "run");

  if (rbuff == NULL) {
    rsz = MKC_SMALL_BUFF_SZ;
    rbuff = malloc (rsz);
    if (rbuff == NULL) {
      mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return MKC_ERR_FAILURE;
    }
    rallocated = true;
  }

  rc = mkc_os_process_pipe (check->targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);

  mkc_log (check->log, MKC_LOG_CHECK, "  run: rc: %d\n", rc);
  if (retsz > 0) {
    mkc_log (check->log, MKC_LOG_CHECK, "--- run log\n");
    mkc_log (check->log, MKC_LOG_CHECK, "%s\n", rbuff);
    mkc_log (check->log, MKC_LOG_CHECK, "---\n");
  }

  free (exefile);
  if (rallocated) {
    free (rbuff);
  }

  return rc;
}

