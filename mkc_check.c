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

#include "mkc_check.h"
#include "mkc_compiler.h"
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

typedef struct mkc_check_t {
  mkc_profile_t     * profiles;
  mkc_pvar_t        * pvar;
  mkc_error_t       * mkcerr;
  mkc_log_t         * log;
  mkc_attribute_t   * attr;
  char              ** cf;
  char              ** lf;
  const   char      ** targv;
  char              * pkgname;
  mkc_regex_t       * rxargcount;
  mkc_regex_t       * rxcomma;
  mkc_profidx_t     pidx_internal;
  mkc_profidx_t     pidx_dflt_comp;
  int               cfcount;
  int               cfallocsz;
  int               lfcount;
  int               lfallocsz;
  int               targc;
  int               targvallocsz;
} mkc_check_t;

static void mkc_check_file_sub_copy (mkc_check_t *check, char *tbuff, size_t sz, const char *fname, const char *origsfx, const char *sfx);
static void mkc_check_log_command (mkc_check_t *check, const char *tag);
static mkc_err_code_t mkc_chk_env_var_set (mkc_check_t *check, const char *nm);
static void mkc_check_append_arg (mkc_check_t *check, const char *arg);
const char * mkc_check_get_compstr (mkc_check_t *check, mkc_compiler_t compiler, char *buff, size_t sz);
static int mkc_chk_package_exec (mkc_check_t *check, const char *pkg);

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

  tpidx = mkc_profile_find (check->profiles,
      MKC_PROF_INTERNAL_NAME, MKC_COMPILER_GENERAL);
  check->pidx_internal = tpidx;
  check->pidx_dflt_comp = pidx;

  check->cf = NULL;
  check->cfcount = 0;
  check->cfallocsz = 10;
  check->cf = malloc (check->cfallocsz * sizeof (char *));
  if (check->cf == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
  }

  check->lf = NULL;
  check->lfcount = 0;
  check->lfallocsz = 10;
  check->lf = malloc (check->lfallocsz * sizeof (char *));
  if (check->lf == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
  }

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

  for (int i = 0; i < check->cfcount; ++i) {
    if (check->cf [i] != NULL) {
      free (check->cf [i]);
    }
  }
  for (int i = 0; i < check->lfcount; ++i) {
    if (check->lf [i] != NULL) {
      free (check->lf [i]);
    }
  }

  datafree (check->cf);
  datafree (check->lf);
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
  /* 2026-5-29 when this is run, the compiler is not yet known */
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

  for (int i = 0; i < check->cfcount; ++i) {
    if (check->cf [i] != NULL) {
      free (check->cf [i]);
    }
  }
  for (int i = 0; i < check->lfcount; ++i) {
    if (check->lf [i] != NULL) {
      free (check->lf [i]);
    }
  }

  check->cf [0] = NULL;
  check->lf [0] = NULL;
  check->targv [0] = NULL;
  check->cfcount = 0;
  check->lfcount = 0;
  check->targc = 0;
}

void
mkc_chk_append_comp_flag (mkc_check_t *check, const char *flag)
{
  if (check == NULL || flag == NULL) {
    return;
  }

  if (check->cfcount >= check->cfallocsz) {
    check->cfallocsz += 10;
    check->cf = realloc (check->cf, check->cfallocsz * sizeof (char *));
    if (check->cf == NULL) {
      mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return;
    }
  }

  check->cf [check->cfcount] = strdup (flag);
  check->cfcount += 1;
}

void
mkc_chk_append_link_flag (mkc_check_t *check, const char *flag)
{
  if (check == NULL || flag == NULL) {
    return;
  }

  if (check->lfcount >= check->lfallocsz) {
    check->lfallocsz += 10;
    check->lf = realloc (check->lf, check->lfallocsz * sizeof (char *));
    if (check->lf == NULL) {
      mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return;
    }
  }

  check->lf [check->lfcount] = strdup (flag);
  check->lfcount += 1;
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
  char        inc [MKC_PATH_MAX];
  const char  *flags [3];
  int         fcount = 0;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: system-type\n");

  mkc_path_build (MKC_PATH_MKC_INCLUDE, inc, sizeof (inc), NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  rc = mkc_compile_run (check, compiler, "int-system", flags, NULL, 0);
  mkc_chk_reset (check);
  return rc;
}

int
mkc_chk_system_id (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;
  char        inc [MKC_PATH_MAX];
  const char  *flags [3];
  int         fcount = 0;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: system-id\n");
  mkc_path_build (MKC_PATH_MKC_INCLUDE, inc, sizeof (inc), NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  rc = mkc_compile_run (check, compiler, "int-sysid", flags, NULL, 0);
  mkc_chk_reset (check);
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
  char        inc [MKC_PATH_MAX];
  const char  *flags [3];
  int         fcount = 0;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: lib-location\n");
  mkc_path_build (MKC_PATH_MKC_INCLUDE, inc, sizeof (inc), NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  rc = mkc_compile_run (check, compiler, "int-libloc", flags, NULL, 0);
  mkc_chk_reset (check);
  return rc;
}

int
mkc_chk_compiler_id (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;
  char        inc [MKC_PATH_MAX];
  const char  *flags [3];
  int         fcount = 0;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: compiler-id\n");
  mkc_path_build (MKC_PATH_MKC_INCLUDE, inc, sizeof (inc), NULL, check->mkcerr);
  flags [fcount++] = "-I";
  flags [fcount++] = inc;
  flags [fcount++] = NULL;
  rc = mkc_compile_run (check, compiler, "int-compid", flags, NULL, 0);
  mkc_chk_reset (check);
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
  char            tbuff [MKC_VNAME_MAX];
  int             fcount = 0;
  char            **match;
  int             matchcount = 0;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: arg_count: %s\n", funcname);
  opidx = mkc_profile_get_active (check->profiles);

  mkc_pvar_profile_set_idx (check->pvar, check->pidx_internal);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_FUNCTION_NAME", funcname, MKC_VCTXT_TEMP);
  mkc_pvar_profile_set_idx (check->pvar, opidx);

  rbuff = malloc (rsz);
  if (rbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_OK;
  }

  flags [fcount++] = "-E";
  flags [fcount++] = NULL;
  rc = mkc_compile_only (check, compiler, "c-argcount", flags, rbuff, rsz);
//fprintf (stderr, "rbuff: %zd\n%s\n", rsz, rbuff);

  /*  int mkdir (const char *__path, __mode_t __mode) */
  /*      ;   */

#if _have_regex
  if (check->rxargcount == NULL) {
    snprintf (tbuff, sizeof (tbuff),
        "([ \t]+%s[ \t]\\([^)]*\\)[ \t\r\n]*;)", funcname);
    check->rxargcount = mkc_regex_init (tbuff, check->mkcerr);
    check->rxcomma = mkc_regex_init ("(,)", check->mkcerr);
  }

  match = mkc_regex_get (check->rxargcount, rbuff);
  /* match[1] will contain the string if matched */
  matchcount = 0;
  while (match [matchcount] != NULL) {
    ++matchcount;
  }
  if (matchcount == 3) {
    const char  *tmatch;
    char        **cmatch;

    /* now count the number of commas */
    tmatch = match [1];
    cmatch = mkc_regex_get (check->rxcomma, tmatch);
    matchcount = 0;
    while (cmatch [matchcount] != NULL) {
      ++matchcount;
    }

    /* the match return includes the before and trailing strings, */
    /* subtract 2 */
    /* add 1 for the number of arguments */
    rc = matchcount - 2 + 1;
    mkc_regex_get_free (cmatch);
  }

  mkc_regex_get_free (match);
#endif

  mkc_chk_reset (check);
  return rc;
}

int
mkc_chk_compiler_flag (mkc_check_t *check,
    mkc_compiler_t compiler,
    const char *flag, bool negate)
{
  int               rc;
  char              tbuff [100];
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
  rc = mkc_compile_only (check, compiler, "int-main", NULL, rbuff, rsz);
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

  mkc_pvar_profile_set_idx (check->pvar, check->pidx_internal);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_CONSTANT", consttxt, MKC_VCTXT_TEMP);

  mkc_pvar_profile_set_idx (check->pvar, opidx);

  rc = mkc_compile_only (check, compiler, "c-const", NULL, NULL, 0);
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

  mkc_pvar_profile_set_idx (check->pvar, check->pidx_internal);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_DEFINE", def, MKC_VCTXT_TEMP);

  mkc_pvar_profile_set_idx (check->pvar, opidx);

  rc = mkc_compile_only (check, compiler, "c-define", NULL, NULL, 0);
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
  char              rbuff [400];

  mkc_chk_append_link_flag (check, flag);
  mkc_log (check->log, MKC_LOG_CHECK, "== chk: link-flag: %s\n", flag);
  rc = mkc_compile_link (check, compiler,
      "int-main", NULL, rbuff, sizeof (rbuff));
  if (rc == 0) {
    /* clang does not return an error code on a unknown warning */
    if (strstr (rbuff, "warning") != NULL) {
      rc = 1;
    }
  }
  mkc_chk_reset (check);
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

  mkc_pvar_profile_set_idx (check->pvar, check->pidx_internal);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_SIZE", type, MKC_VCTXT_TEMP);

  mkc_pvar_profile_set_idx (check->pvar, opidx);

  rc = mkc_compile_run (check, compiler, "c-size", NULL, NULL, 0);
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

  mkc_pvar_profile_set_idx (check->pvar, check->pidx_internal);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_TYPE", type, MKC_VCTXT_TEMP);

  mkc_pvar_profile_set_idx (check->pvar, opidx);

  rc = mkc_compile_only (check, compiler, "c-type", NULL, NULL, 0);
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

  mkc_pvar_profile_set_idx (check->pvar, check->pidx_internal);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_STRUCT_NAME", structname, MKC_VCTXT_TEMP);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_STRUCT_MEMBER", membername, MKC_VCTXT_TEMP);

  mkc_pvar_profile_set_idx (check->pvar, opidx);

  rc = mkc_compile_only (check, compiler, "c-struct-member", NULL, NULL, 0);
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

  mkc_pvar_profile_set_idx (check->pvar, check->pidx_internal);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_FUNCTION_NAME", funcname, MKC_VCTXT_TEMP);

  mkc_pvar_profile_set_idx (check->pvar, opidx);

  rc = mkc_compile_link (check, compiler, "c-function", NULL, NULL, 0);
  mkc_chk_reset (check);
  return rc;
}


int
mkc_compile_only (mkc_check_t *check, mkc_compiler_t compiler,
    const char *fname, const char *flags [], char *rbuff, size_t rsz)
{
  int         rc;
  char        tbuff [MKC_PATH_MAX];
  size_t      retsz;
  const char  *sfx = NULL;
  char        compstr [MKC_PATH_MAX];
  bool        rallocated = false;
  char        outfile [MKC_PATH_MAX];
  bool        cpreprocess = false;

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

  mkc_check_get_compstr (check, compiler, compstr, sizeof (compstr));
  sfx = mkc_compiler_get_suffix (compiler);
// ### will need to be fixed, the original suffix may change
  mkc_check_file_sub_copy (check, tbuff, sizeof (tbuff), fname, ".c", sfx);

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
  for (int i = 0; i < check->cfcount; ++i) {
    mkc_check_append_arg (check, check->cf [i]);
  }
  if (! cpreprocess) {
    mkc_check_append_arg (check, "-c");
    mkc_check_append_arg (check, "-o");
    mkc_path_build (MKC_PATH_MKC_TMP, outfile, sizeof (outfile), "mkctest.o", check->mkcerr);
    mkc_check_append_arg (check, outfile);
  }
  mkc_check_append_arg (check, tbuff);
  mkc_check_append_arg (check, NULL);
  if (mkc_error_chk_err (check->mkcerr)) {
    if (rallocated) {
      free (rbuff);
    }
    return MKC_ERR_FAILURE;
  }

  mkc_check_log_command (check, "comp-only");

  rc = mkc_os_process_pipe (check->targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);

  if (retsz > 0) {
    mkc_log (check->log, MKC_LOG_CHECK, "--- compile log\n");
    mkc_log (check->log, MKC_LOG_CHECK, "%s\n", rbuff);
    mkc_log (check->log, MKC_LOG_CHECK, "---\n");
  }
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);

  /* never want the return code to overlap with various enums */
  if (rc > 0) {
    rc = - rc;
  }

  if (rallocated) {
    free (rbuff);
  }
  return rc;
}

int
mkc_compile_link (mkc_check_t *check, mkc_compiler_t compiler,
    const char *fname, const char *flags [],
    char *rbuff, size_t rsz)
{
  int         rc;
  size_t      retsz;
  bool        rallocated = false;
  char        compstr [MKC_PATH_MAX];
  char        outfile [MKC_PATH_MAX];
  char        objfile [MKC_PATH_MAX];

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

  mkc_check_get_compstr (check, compiler, compstr, sizeof (compstr));

  check->targc = 0;
  mkc_check_append_arg (check, compstr);
  mkc_check_append_arg (check, "-o");
  mkc_path_build (MKC_PATH_MKC_TMP, outfile, sizeof (outfile), "mkctest.exe", check->mkcerr);
  mkc_check_append_arg (check, outfile);
  mkc_path_build (MKC_PATH_MKC_TMP, objfile, sizeof (objfile), "mkctest.o", check->mkcerr);
  mkc_check_append_arg (check, objfile);
  for (int i = 0; i < check->lfcount; ++i) {
    mkc_check_append_arg (check, check->lf [i]);
  }
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

  if (rallocated) {
    free (rbuff);
  }

  return rc;
}

int
mkc_compile_run (mkc_check_t *check,
    mkc_compiler_t compiler,
    const char *fname, const char *flags [],
    char *rbuff, size_t rsz)
{
  int         rc;
  bool        rallocated = false;
  size_t      retsz;
  char        exefile [MKC_PATH_MAX];

  rc = mkc_compile_link (check, compiler, fname, flags, NULL, 0);

  if (rc != 0) {
    return rc;
  }

  check->targc = 0;
  mkc_path_build (MKC_PATH_MKC_TMP, exefile, sizeof (exefile), "mkctest.exe", check->mkcerr);
  mkc_check_append_arg (check, exefile);
  mkc_check_append_arg (check, NULL);
  if (mkc_error_chk_err (check->mkcerr)) {
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

  if (rallocated) {
    free (rbuff);
  }

  return rc;
}

static void
mkc_check_file_sub_copy (mkc_check_t *check,
    char *tbuff, size_t sz,
    const char *fname, const char *origsfx, const char *sfx)
{
  char    tfn [MKC_VNAME_MAX];
  char    fbuff [MKC_PATH_MAX];
  char    *data;
  char    *ndata;
  FILE    *fh;
  size_t  fsz;

  snprintf (tfn, sizeof (tfn), "%s%s", fname, origsfx);
  mkc_path_build (MKC_PATH_MKC_TEMPLATES, fbuff, sizeof (fbuff), tfn, check->mkcerr);
  data = mkc_read_file (fbuff, &fsz, check->mkcerr);
  if (mkc_error_chk_err (check->mkcerr)) {
    return;
  }
  ndata = mkc_pvar_substitute (check->pvar, data, 0);
  mkc_log (check->log, MKC_LOG_CHECK, "--- code:\n");
  mkc_log (check->log, MKC_LOG_CHECK, "%s\n", ndata);
  mkc_log (check->log, MKC_LOG_CHECK, "---\n");
  free (data);

  snprintf (tfn, sizeof (tfn), "%s%s", fname, sfx);
  mkc_path_build (MKC_PATH_MKC_TMP, tbuff, sz, tfn, check->mkcerr);

  fh = mkc_fopen (tbuff, "wb");
  fwrite (ndata, strlen (ndata), 1, fh);
  fclose (fh);
  free (ndata);
}

static void
mkc_check_log_command (mkc_check_t *check, const char *tag)
{
  int   targc;

  targc = 0;
  mkc_log (check->log, MKC_LOG_CHECK, "cmd: ");
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
}

static mkc_err_code_t
mkc_chk_env_var_set (mkc_check_t *check, const char *nm)
{
  char            tbuff [MKC_PATH_MAX];
  mkc_err_code_t  rc = MKC_OK;

  *tbuff = '\0';
  mkc_env_get (nm, tbuff, sizeof (tbuff));
  if (*tbuff) {
    rc = mkc_pvar_set_str (check->pvar, nm, tbuff, MKC_VCTXT_ENV);
  }

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

const char *
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
  mkc_value_t   *value;
  char          pkgconfpath [MKC_PATH_MAX];
  char          tmpname [MKC_VNAME_MAX];
  char          *rbuff;
  size_t        rsz;
  size_t        retsz;
  int           rc;

  *pkgconfpath = '\0';
  value = mkc_pvar_get_by_profidx (check->pvar, mkcpathpkgconf, check->pidx_internal);
  if (value == NULL) {
    value = mkc_pvar_get_by_profidx (check->pvar, mkcpathpkgconfig, check->pidx_internal);
  }
  if (value != NULL) {
    mkc_pvar_value_get_str (check->pvar, value, pkgconfpath, sizeof (pkgconfpath));
  }

  if (! *pkgconfpath) {
    mkc_error_set (check->mkcerr, MKC_ERR_PKGCONF_NOT_FOUND, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  check->targc = 0;
  mkc_check_append_arg (check, pkgconfpath);
  if (check->attr->path != NULL) {
    mkc_check_append_arg (check, "--with-path");
    mkc_check_append_arg (check, check->attr->path);
  }
  mkc_check_append_arg (check, "--exists");
  mkc_check_append_arg (check, pkg);
  mkc_check_append_arg (check, NULL);
  if (mkc_error_chk_err (check->mkcerr)) {
    return MKC_ERR_FAILURE;
  }

  rsz = MKC_SMALL_BUFF_SZ;
  rbuff = malloc (rsz);
  if (rbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }
  mkc_check_log_command (check, "pkg-exists");

  rc = mkc_os_process_pipe (check->targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);
  mkc_log (check->log, MKC_LOG_CHECK, "pkg exists: %s\n", pkg);
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (rc != MKC_OK) {
    free (rbuff);
    return rc;
  }

  check->targc = 0;
  mkc_check_append_arg (check, pkgconfpath);
  if (check->attr->path != NULL) {
    mkc_check_append_arg (check, "--with-path");
    mkc_check_append_arg (check, check->attr->path);
  }
  mkc_check_append_arg (check, "--cflags");
  mkc_check_append_arg (check, pkg);
  mkc_check_append_arg (check, NULL);
  if (mkc_error_chk_err (check->mkcerr)) {
    free (rbuff);
    mkc_chk_reset (check);
    return MKC_ERR_FAILURE;
  }

  mkc_check_log_command (check, "pkg-cflags");

  rc = mkc_os_process_pipe (check->targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);
  mkc_log (check->log, MKC_LOG_CHECK, "pkg cflags: %s\n", rbuff);
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (rc != MKC_OK) {
    free (rbuff);
    mkc_chk_reset (check);
    return rc;
  }
  if (retsz > 0) {
    const char  *tmp;

    tmp = pkg;
    if (check->attr->name != NULL) {
      tmp = check->attr->name;
    }
    mkc_strtrim (rbuff, retsz);
    snprintf (tmpname, sizeof (tmpname), "%s_CFLAGS", tmp);
    mkc_strclean (tmpname, 0);
    mkc_pvar_set_list_from_str (check->pvar, tmpname, rbuff, MKC_VCTXT_CHECK);
  }

  mkc_chk_reset (check);
  check->targc = 0;
  mkc_check_append_arg (check, pkgconfpath);
  if (check->attr->path != NULL) {
    mkc_check_append_arg (check, "--with-path");
    mkc_check_append_arg (check, check->attr->path);
  }
  mkc_check_append_arg (check, "--libs");
  mkc_check_append_arg (check, pkg);
  mkc_check_append_arg (check, NULL);
  if (mkc_error_chk_err (check->mkcerr)) {
    free (rbuff);
    mkc_chk_reset (check);
    return MKC_ERR_FAILURE;
  }

  mkc_check_log_command (check, "pkg-libs");

  rc = mkc_os_process_pipe (check->targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);
  mkc_log (check->log, MKC_LOG_CHECK, "pkg libs: %s\n", rbuff);
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (retsz > 0) {
    const char  *tmp;

    tmp = pkg;
    if (check->attr->name != NULL) {
      tmp = check->attr->name;
    }
    mkc_strtrim (rbuff, retsz);
    snprintf (tmpname, sizeof (tmpname), "%s_LIBS", tmp);
    mkc_strclean (tmpname, 0);
    mkc_pvar_set_list_from_str (check->pvar, tmpname, rbuff, MKC_VCTXT_CHECK);
  }

  mkc_chk_reset (check);
  free (rbuff);
  return rc;
}
