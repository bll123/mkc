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
#include <unistd.h>

#include "alternate.h"
#include "attribute.h"
#include "chararr.h"
#include "mkc_check.h"
#include "mkc_compiler.h"
#include "const.h"
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
  scopedvar_t       * sv;
  compile_t         * compile;
  mkc_error_t       * mkcerr;
  mkc_log_t         * log;
  mkc_attribute_t   * attr;
  char              * pkgname;
  mkc_regex_t       * rxargcount;
  mkc_regex_t       * rxcomma;
  chararr_t         * flags;
} mkc_check_t;

static mkc_err_code_t mkc_chk_env_var_set (mkc_check_t *check, const char *nm);
static int mkc_chk_package_exec (mkc_check_t *check, const char *pkgconfpath, const char *flag, const char *pkg, chararr_t * targv, char *rbuff, size_t rsz, const char *name);

MKC_NODISCARD
mkc_check_t *
mkc_check_init (scopedvar_t *sv, compile_t *compile,
    mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr)
{
  mkc_check_t   *check;

  check = malloc (sizeof (mkc_check_t));
  check->sv = sv;
  check->compile = compile;
  check->attr = attr;
  check->mkcerr = mkcerr;
  check->log = log;
  check->pkgname = NULL;
  check->rxargcount = NULL;
  check->rxcomma = NULL;

  check->flags = chararr_init (mkcerr);
  if (check->flags == NULL) {
    return NULL;
  }

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
  chararr_free (check->flags);

  free (check);
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
  const char  * nodeprecateflag;

  /* clang prints the deprecated error when compiling C with */
  /* c++ or objective-c */

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: compiler-works\n");
  compile_usetemplate (check->compile);
  nodeprecateflag =
      compiler_get_flag (check->attr->compid, MKC_COMP_FLAG_WARN_NO_DEPRECATE);
  if (*nodeprecateflag) {
    compile_append_flag (check->compile, COMP_COMPFLAGS, nodeprecateflag);
  }
  compile_append_flag (check->compile, COMP_COMPFLAGS, NULL);
  rc = compile_exec (check->compile, COMPILE_COMPILE, compiler,
      "int-main", NULL, 0);
  compile_reset (check->compile);
  return rc;
}

int
mkc_chk_header_modern (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: header-modern\n");
  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE, compiler,
      "int-header-modern", NULL, 0);
  compile_reset (check->compile);
  return rc;
}

int
mkc_chk_system_type (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc = MKC_ERR_FAILURE;
  int         systype = MKC_ERR_FAILURE;
  char        *inc;
  char        rbuff [MKC_VNAME_MAX];

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: system-type\n");

  inc = malloc (MKC_PATH_MAX);
  if (inc == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_SYS_UNKNOWN;
  }
  path_build (MKC_PATH_MKC_SHR_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);


  chararr_append (check->flags,
      compiler_get_flag (check->attr->compid, MKC_COMP_FLAG_INCLUDE));
  chararr_append (check->flags, inc);
  chararr_append (check->flags, NULL);
  compile_set_flags (check->compile, check->flags, NULL, NULL);
  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE_LINK_RUN, compiler,
      "int-system", rbuff, sizeof (rbuff));
  if (rc == 0) {
    systype = atoi (rbuff);
  }
  compile_reset (check->compile);
  chararr_reset (check->flags, 0);
  free (inc);
  return systype;
}

int
mkc_chk_system_id (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc = MKC_ERR_FAILURE;
  int         sysid = MKC_ERR_FAILURE;
  char        *inc;
  char        rbuff [MKC_VNAME_MAX];

  inc = malloc (MKC_PATH_MAX);
  if (inc == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_SYS_ID_NOTSET;
  }
  mkc_log (check->log, MKC_LOG_CHECK, "== chk: system-id\n");
  path_build (MKC_PATH_MKC_SHR_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);
  chararr_append (check->flags,
      compiler_get_flag (check->attr->compid, MKC_COMP_FLAG_INCLUDE));
  chararr_append (check->flags, inc);
  chararr_append (check->flags, NULL);
  compile_set_flags (check->compile, check->flags, NULL, NULL);
  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE_LINK_RUN, compiler,
      "int-sysid", rbuff, sizeof (rbuff));
  if (rc == 0) {
    sysid = atoi (rbuff);
  }
  compile_reset (check->compile);
  chararr_reset (check->flags, 0);
  free (inc);
  return sysid;
}

int
mkc_chk_variadic_macro (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: variadic-macro\n");
  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE, compiler,
      "int-variadic-macro", NULL, 0);
  compile_reset (check->compile);
  return rc;
}

/* the library location is used for linux systems */
/* some linux systems use lib64 as the main library suffix */
/* other linux systems have lib64, but only use it for lib64 specific */
/* libraries */
int
mkc_chk_library_location (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc = MKC_ERR_FAILURE;
  int         libloc = MKC_ERR_FAILURE;
  char        *inc;
  char        rbuff [MKC_VNAME_MAX];

  inc = malloc (MKC_PATH_MAX);
  if (inc == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return rc;
  }
  mkc_log (check->log, MKC_LOG_CHECK, "== chk: lib-location\n");
  path_build (MKC_PATH_MKC_SHR_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);
  chararr_append (check->flags,
      compiler_get_flag (check->attr->compid, MKC_COMP_FLAG_INCLUDE));
  chararr_append (check->flags, inc);
  chararr_append (check->flags, NULL);
  compile_set_flags (check->compile, check->flags, NULL, NULL);
  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE_LINK_RUN, compiler,
      "int-libloc", rbuff, sizeof (rbuff));
  if (rc == 0) {
    libloc = atoi (rbuff);
  }
  compile_reset (check->compile);
  chararr_reset (check->flags, 0);
  free (inc);
  return libloc;
}

int
mkc_chk_compiler_id (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc = MKC_ERR_FAILURE;
  int         compid = MKC_ERR_FAILURE;
  char        *inc;
  char        rbuff [MKC_VNAME_MAX];

  inc = malloc (MKC_PATH_MAX);
  if (inc == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return rc;
  }
  mkc_log (check->log, MKC_LOG_CHECK, "== chk: compiler-id\n");
  path_build (MKC_PATH_MKC_SHR_INCLUDE, inc, MKC_PATH_MAX, NULL, check->mkcerr);
  chararr_append (check->flags,
      compiler_get_flag (check->attr->compid, MKC_COMP_FLAG_INCLUDE));
  chararr_append (check->flags, inc);
  chararr_append (check->flags, NULL);
  compile_set_flags (check->compile, check->flags, NULL, NULL);
  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE_LINK_RUN, compiler,
      "int-compid", rbuff, sizeof (rbuff));
  if (rc == 0) {
    compid = atoi (rbuff);
  }
  compile_reset (check->compile);
  chararr_reset (check->flags, 0);
  free (inc);
  return compid;
}

int
mkc_chk_getconf (mkc_check_t *check)
{
  int     rc = MKC_ERR_FAILURE;

  /* getconf LFS_CFLAGS supports is far less prevalent than I expected */
  /* e.g. getconf LFS_CFLAGS no longer works on FreeBSD */
  /* in any case, this will work on Linux */
  /* macos does not have _CS_LFS_CFLAGS defined */
  /* getconf POSIX_V6_LPBIG_OFFBIG_CFLAGS on macos returns invalid flags */
#if _function_confstr && _define__CS_LFS_CFLAGS
  char    flag [MKC_VNAME_MAX];
  size_t  rsz;

  *flag = '\0';
  rsz = confstr (_CS_LFS_CFLAGS, flag, sizeof (flag));
  if (rsz > 0 && *flag) {
    sv_append_str_list (check->sv, SV_T_ACTIVE,
        MKC_C_COMPFLAGS, NULL, flag, MKC_VCTXT_MKC);
  }

  *flag = '\0';
  rsz = confstr (_CS_LFS_LDFLAGS, flag, sizeof (flag));
  if (rsz > 0 && *flag) {
    sv_append_str_list (check->sv, SV_T_ACTIVE,
        MKC_C_LINKFLAGS, NULL, flag, MKC_VCTXT_MKC);
  }

  *flag = '\0';
  rsz = confstr (_CS_LFS_LDFLAGS, flag, sizeof (flag));
  if (rsz > 0 && *flag) {
    sv_append_str_list (check->sv, SV_T_ACTIVE,
        MKC_C_LIBS, NULL, flag, MKC_VCTXT_MKC);
  }
#endif

  return rc;
}

int
mkc_chk_arg_count (mkc_check_t *check, mkc_compiler_t compiler,
    const char *funcname)
{
  int             rc = 0;
  char            *rbuff;
  size_t          rsz = MKC_LARGE_BUFF_SZ;
#if _have_regex
  char            pattern [MKC_VNAME_MAX];
  char            **match;
  int             matchcount = 0;
#endif

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: arg_count: %s\n", funcname);

  sv_set_str (check->sv, SV_T_LOCAL, "MKC_TV_TEST_FUNCTION_NAME", NULL, funcname, MKC_VCTXT_TEMP);

  rbuff = malloc (rsz);
  if (rbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_OK;
  }

  compile_preprocess (check->compile);
  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE, compiler,
      "c-argcount", rbuff, rsz);
  compile_reset (check->compile);

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
  static const char *negprefix;
  static const char *warnprefix;
  static size_t     neglen;

  negprefix = compiler_get_flag (check->attr->compid, MKC_COMP_FLAG_WARN_NEGATE);
  neglen = compiler_get_flag_len (check->attr->compid, MKC_COMP_FLAG_WARN_NEGATE);
  warnprefix = compiler_get_flag (check->attr->compid, MKC_COMP_FLAG_WARN_PREFIX);

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
      p = stpecpy (tbuff, tbuff + sizeof (tbuff), warnprefix);
      p = stpecpy (p, tbuff + sizeof (tbuff), flag + neglen);
    }
  }

  compile_set_flags (check->compile, NULL, NULL, NULL);
  compile_usetemplate (check->compile);
  compile_append_flag (check->compile, COMP_COMPFLAGS, tbuff);
  compile_append_flag (check->compile, COMP_COMPFLAGS, NULL);
  rc = compile_exec (check->compile, COMPILE_COMPILE, compiler,
      "c-main", rbuff, rsz);
  compile_reset (check->compile);
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

  sv_set_str (check->sv, SV_T_LOCAL, "MKC_TV_TEST_CONSTANT", NULL, consttxt, MKC_VCTXT_TEMP);

  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE, compiler,
      "c-const", NULL, 0);
  compile_reset (check->compile);
  return rc;
}

int
mkc_chk_define (mkc_check_t *check,
    mkc_compiler_t compiler, const char *def)
{
  int             rc;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: define: %s\n", def);

  sv_set_str (check->sv, SV_T_LOCAL, "MKC_TV_TEST_DEFINE", NULL, def, MKC_VCTXT_TEMP);

  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE, compiler,
      "c-define", NULL, 0);
  compile_reset (check->compile);
  return rc;
}

int
mkc_chk_package (mkc_check_t *check,
    mkc_compiler_t compiler, const char *pkg)
{
  int             rc = MKC_ERR_FAILURE;
  char            * pkgconfpath;
  value_t         * value;
  chararr_t       * targv;
  int             btargc;
  char            * tpath;
  const char      * tmpnm;
  mkc_alternate_t * alt;
  listidx_t   iteridx;
  listidx_t   pathidx;
  char            tmpname [MKC_VNAME_MAX];
  char            * rbuff;

  pkgconfpath = malloc (MKC_PATH_MAX);
  if (pkgconfpath == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  *pkgconfpath = '\0';
  /* if pkgconf is installed, pkg-config is a symlink. */
  /* use pkg-config by preference (pkgconf does not seem to work in macos macports) */
  value = sv_get_value (check->sv, SV_T_INTERNAL, MKC_C_PATH_PKGCONFIG, NULL);
  if (value == NULL) {
    value = sv_get_value (check->sv, SV_T_INTERNAL, MKC_C_PATH_PKGCONF, NULL);
  }
  if (value != NULL) {
    sv_value_get_str (check->sv, value, pkgconfpath, MKC_PATH_MAX);
  }

  if (! *pkgconfpath) {
    mkc_error_set (check->mkcerr, MKC_ERR_PKGCONF_NOT_FOUND, 0, NULL);
    free (pkgconfpath);
    return rc;
  }

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: package: %s\n", pkg);

  datafree (check->pkgname);
  check->pkgname = strdup (pkg);

  targv = chararr_init (check->mkcerr);
  if (targv == NULL) {
    return rc;
  }
  chararr_append (targv, pkgconfpath);

  tpath = malloc (MKC_PATH_MAX);
  if (tpath == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (pkgconfpath);
    return MKC_ERR_FAILURE;
  }

  list_iter_start (check->attr->pathlist, &iteridx);
  while ((pathidx = list_iter_next (check->attr->pathlist, &iteridx)) != MKC_ITER_FINISH) {
    value_t   *path;

    if (mkc_error_chk_err (check->mkcerr)) {
      free (pkgconfpath);
      free (tpath);
      return MKC_ERR_FAILURE;
    }

    path = list_get_by_idx (check->attr->pathlist, pathidx);
    sv_value_get_str (check->sv, path, tpath, MKC_PATH_MAX);
    if (*tpath) {
      chararr_append (targv, "--with-path");
      chararr_append (targv, tpath);
    }
  }

  if (mkc_error_chk_err (check->mkcerr)) {
    free (pkgconfpath);
    free (tpath);
    chararr_free (targv);
    return MKC_ERR_FAILURE;
  }

  rbuff = malloc (MKC_SMALL_BUFF_SZ);
  if (rbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (pkgconfpath);
    free (tpath);
    chararr_free (targv);
    return MKC_ERR_FAILURE;
  }

  alt = check->attr->curralt;
  tmpnm = pkg;
  if (alt->name != NULL) {
    tmpnm = alt->name;
  }

  btargc = chararr_size (targv);

  /* libpkgconf's api is far to complex to bother using. */
  rc = mkc_chk_package_exec (check, pkgconfpath, "--exists", pkg,
      targv, rbuff, MKC_SMALL_BUFF_SZ, NULL);
  if (rc != MKC_OK) {
    free (pkgconfpath);
    free (tpath);
    free (rbuff);
    chararr_free (targv);
    return rc;
  }

  chararr_reset (targv, btargc);
  snprintf (tmpname, sizeof (tmpname), "%s_CFLAGS", tmpnm);
  str_clean (tmpname, 0);

  rc = mkc_chk_package_exec (check, pkgconfpath, "--cflags", pkg,
      targv, rbuff, MKC_SMALL_BUFF_SZ, tmpname);

  chararr_reset (targv, btargc);
  snprintf (tmpname, sizeof (tmpname), "%s_LIBS", tmpnm);
  str_clean (tmpname, 0);

  rc = mkc_chk_package_exec (check, pkgconfpath, "--libs", pkg,
      targv, rbuff, MKC_SMALL_BUFF_SZ, tmpname);

  chararr_free (targv);
  free (pkgconfpath);
  free (tpath);
  free (rbuff);
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

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: link-flag: %s\n", flag);
  compile_append_flag (check->compile, COMP_LINKFLAGS, flag);
  compile_append_flag (check->compile, COMP_LINKFLAGS, NULL);
  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE_LINK, compiler,
      "c-main", rbuff, MKC_PATH_MAX);
  compile_reset (check->compile);
  if (rc == 0) {
    /* clang does not return an error code on a unknown warning */
    if (strstr (rbuff, "warning") != NULL) {
      rc = 1;
    }
  }
  free (rbuff);
  return rc;
}

/* return a size of 0 when the type is not found */
int
mkc_chk_size (mkc_check_t *check,
    mkc_compiler_t compiler, const char *type)
{
  int       rc;
  int       sz = 0;
  char      rbuff [MKC_VNAME_MAX];

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: size: %s\n", type);

  sv_set_str (check->sv, SV_T_LOCAL, "MKC_TV_TEST_SIZE", NULL, type, MKC_VCTXT_TEMP);

  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE_LINK_RUN, compiler,
      "c-size", rbuff, sizeof (rbuff));
  if (rc == 0) {
    sz = atoi (rbuff);
  }
  compile_reset (check->compile);
  return sz;
}

int
mkc_chk_type (mkc_check_t *check,
    mkc_compiler_t compiler, const char *type)
{
  int             rc;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: type: %s\n", type);

  sv_set_str (check->sv, SV_T_LOCAL, "MKC_TV_TEST_TYPE", NULL, type, MKC_VCTXT_TEMP);

  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE, compiler,
      "c-type", NULL, 0);
  compile_reset (check->compile);
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

  sv_set_str (check->sv, SV_T_LOCAL, "MKC_TV_TEST_STRUCT_NAME", NULL, structname, MKC_VCTXT_TEMP);
  sv_set_str (check->sv, SV_T_LOCAL, "MKC_TV_TEST_STRUCT_MEMBER", NULL, membername, MKC_VCTXT_TEMP);

  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE, compiler,
      "c-struct-member", NULL, 0);
  compile_reset (check->compile);
  return rc;
}

int
mkc_chk_function (mkc_check_t *check, mkc_compiler_t compiler,
    const char *funcname)
{
  int             rc;

  mkc_log (check->log, MKC_LOG_CHECK,
      "== chk: function: %s\n", funcname);

  sv_set_str (check->sv, SV_T_LOCAL, "MKC_TV_TEST_FUNCTION_NAME", NULL, funcname, MKC_VCTXT_TEMP);

  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE_LINK, compiler,
      "c-function", NULL, 0);
  compile_reset (check->compile);
  return rc;
}

int
mkc_chk_header (mkc_check_t *check, mkc_compiler_t compiler,
    const char *header, chararr_t * compflags)
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
  sv_set_str (check->sv, SV_T_LOCAL, "MKC_TV_TEST_HEADER", NULL, tbuff, MKC_VCTXT_TEMP);

  compile_set_flags (check->compile, compflags, NULL, NULL);
  compile_usetemplate (check->compile);
  rc = compile_exec (check->compile, COMPILE_COMPILE, compiler,
      "c-header", NULL, 0);
  compile_reset (check->compile);
  return rc;
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
    rc = sv_set_str (check->sv, SV_T_INTERNAL, nm, NULL, tbuff, MKC_VCTXT_ENV);
  }

  free (tbuff);

  return rc;
}

static int
mkc_chk_package_exec (mkc_check_t *check, const char *pkgconfpath,
    const char *flag, const char *pkg, chararr_t * targv,
    char *rbuff, size_t rsz, const char *name)
{
  size_t            retsz;
  int               rc;

  chararr_append (targv, flag);
  chararr_append (targv, pkg);
  chararr_append (targv, NULL);

  mkc_log_chararr (check->log, "pkg: cmd: ", targv);

  rc = os_process_pipe (chararr_get_arr (targv),
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (rc != MKC_OK) {
    return rc;
  }

  if (name != NULL) {
    /* make sure a list exists */
    sv_append_str_list (check->sv, SV_T_SEARCH, name, NULL, NULL, MKC_VCTXT_MKC);
    if (retsz > 0) {
      str_trim (rbuff, retsz);
      sv_set_list_from_str (check->sv, name, rbuff, MKC_VCTXT_MKC);
    }
  }

  return rc;
}

