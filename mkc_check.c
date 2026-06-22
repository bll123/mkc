/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#if ! MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if _package_libpkgconf
# include <libpkgconf/libpkgconf.h>
#endif

#include "mkc_check.h"
#include "mkc_compiler.h"
#include "mkc_def.h"
#include "mkc_env.h"
#include "mkc_error.h"
#include "mkc_fileop.h"
#include "mkc_log.h"
#include "mkc_pvar.h"
#include "mkc_os_process.h"
#include "mkc_string.h"
#include "mkc_util.h"

static const char *MKC_INCLUDE_PATH = "include";

typedef struct mkc_check_t {
  mkc_profile_t   * profiles;
  mkc_pvar_t      * pvar;
  mkc_error_t     * mkcerr;
  mkc_log_t       * log;
  char            ** cf;
  char            ** lf;
  const char      ** targv;
  char            * pkgname;
  mkc_profidx_t   pidx_internal;
  mkc_profidx_t   pidx_global_general;
  int             cfcount;
  int             cfallocsz;
  int             lfcount;
  int             lfallocsz;
  int             targc;
  int             targvallocsz;
} mkc_check_t;

static void mkc_check_file_sub_copy (mkc_check_t *check, char *tbuff, size_t sz, const char *fname, const char *sfx);
static void mkc_check_log_command (mkc_check_t *check, const char *tag);
static mkc_err_code_t mkc_chk_env_var_set (mkc_check_t *check, const char *nm);
static void mkc_check_append_arg (mkc_check_t *check, const char *arg);
const char * mkc_check_get_compstr (mkc_check_t *check, mkc_compiler_t compiler, char *buff, size_t sz);
#if _package_libpkgconf
static int mkc_chk_package_lib (mkc_check_t *check, const char *pkg);
#endif
static int mkc_chk_package_exec (mkc_check_t *check, const char *pkg);

mkc_check_t *
mkc_check_init (mkc_profile_t *profiles, mkc_pvar_t *pvar,
    mkc_log_t *log, mkc_profidx_t pidx, mkc_error_t *mkcerr)
{
  mkc_check_t   *check;
  mkc_profidx_t tpidx;

  check = malloc (sizeof (mkc_check_t));
  check->profiles = profiles;
  check->pvar = pvar;
  check->mkcerr = mkcerr;
  check->log = log;
  check->pidx_global_general = pidx;
  check->pkgname = NULL;

  tpidx = mkc_profile_find (check->profiles,
      MKC_PROF_INTERNAL_NAME, MKC_COMPILER_GENERAL);
  check->pidx_internal = tpidx;

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

  if (check->cf != NULL) {
    free (check->cf);
  }
  if (check->lf != NULL) {
    free (check->lf);
  }
  if (check->targv != NULL) {
    free (check->targv);
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

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: system-type\n");
  rc = mkc_compile_run (check, compiler,
      "int-system", MKC_INCLUDE_PATH, NULL, 0);
  mkc_chk_reset (check);
  return rc;
}

int
mkc_chk_system_id (mkc_check_t *check, mkc_compiler_t compiler)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: system-id\n");
  rc = mkc_compile_run (check, compiler,
      "int-sysid", MKC_INCLUDE_PATH, NULL, 0);
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

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: lib-location\n");
  rc = mkc_compile_run (check, compiler,
      "int-libloc", MKC_INCLUDE_PATH, NULL, 0);
  mkc_chk_reset (check);
  return rc;
}

int
mkc_chk_compiler_id (mkc_check_t *check, mkc_compiler_t compiler)
{
  int       rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: compiler-id\n");
  rc = mkc_compile_run (check, compiler,
      "int-compid", MKC_INCLUDE_PATH, NULL, 0);
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

  rsz = MKC_LARGE_BUFF_SZ;
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
  mkc_profidx_t   opidx;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: package: %s\n", pkg);
  opidx = mkc_profile_get_active (check->profiles);

  datafree (check->pkgname);
  check->pkgname = strdup (pkg);
#if _package_libpkgconf
  rc = mkc_chk_package_lib (check, pkg);
#else
  rc = mkc_chk_package_exec (check, pkg);
#endif

  mkc_pvar_profile_set_idx (check->pvar, opidx);

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

  rc = mkc_compile_run (check, compiler,
        "c-size", NULL, NULL, 0);
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
    const char *fname, const char *incpath, char *rbuff, size_t rsz)
{
  int         rc;
  char        tbuff [MKC_PATH_MAX];
  size_t      retsz;
  const char  *sfx = NULL;
  char        compstr [MKC_PATH_MAX];
  bool        rallocated = false;

  if (rbuff == NULL) {
    rsz = MKC_LARGE_BUFF_SZ;
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
  mkc_check_file_sub_copy (check, tbuff, sizeof (tbuff), fname, sfx);

  check->targc = 0;
  mkc_check_append_arg (check, compstr);
  for (int i = 0; i < check->cfcount; ++i) {
    mkc_check_append_arg (check, check->cf [i]);
  }
  if (incpath != NULL) {
    mkc_check_append_arg (check, "-I");
    mkc_check_append_arg (check, incpath);
  }
  mkc_check_append_arg (check, "-c");
  mkc_check_append_arg (check, "-o");
  mkc_check_append_arg (check, "mkc_files/tmp/mkctest.o");
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
    const char *fname, const char *incpath,
    char *rbuff, size_t rsz)
{
  int         rc;
  size_t      retsz;
  bool        rallocated = false;
  char        compstr [MKC_PATH_MAX];

  if (rbuff == NULL) {
    rsz = MKC_LARGE_BUFF_SZ;
    rbuff = malloc (rsz);
    if (rbuff == NULL) {
      mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return MKC_ERR_FAILURE;
    }
    *rbuff = '\0';
    rallocated = true;
  }

  rc = mkc_compile_only (check, compiler, fname, incpath, rbuff, rsz);
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
  mkc_check_append_arg (check, "mkc_files/tmp/mkctest.exe");
  mkc_check_append_arg (check, "mkc_files/tmp/mkctest.o");
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
    const char *fname, const char *incpath,
    char *rbuff, size_t rsz)
{
  int         rc;
  bool        rallocated = false;
  size_t      retsz;

  rc = mkc_compile_link (check, compiler, fname, incpath, NULL, 0);

  if (rc != 0) {
    return rc;
  }

  check->targc = 0;
  mkc_check_append_arg (check, "mkc_files/tmp/mkctest.exe");
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
    const char *fname, const char *sfx)
{
  char    fbuff [MKC_PATH_MAX];
  char    *data;
  char    *ndata;
  FILE    *fh;
  size_t  fsz;

  snprintf (fbuff, sizeof (fbuff), "templates/%s.c", fname);
  data = mkc_read_file (fbuff, &fsz, check->mkcerr);
  if (mkc_error_chk_err (check->mkcerr)) {
    return;
  }
  ndata = mkc_pvar_substitute (check->pvar, data, 0);
  mkc_log (check->log, MKC_LOG_CHECK, "--- code:\n");
  mkc_log (check->log, MKC_LOG_CHECK, "%s\n", ndata);
  mkc_log (check->log, MKC_LOG_CHECK, "---\n");
  free (data);
  snprintf (tbuff, sz, "mkc_files/tmp/%s%s", fname, sfx);
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
  value = mkc_pvar_get_by_profidx (check->pvar, envstr, check->pidx_global_general);
  mkc_pvar_value_get_str (check->pvar, value, buff, sz);
  return buff;
}

static int
mkc_chk_package_exec (mkc_check_t *check, const char *pkg)
{
  mkc_value_t   *value;
  char          pkgconfpath [MKC_PATH_MAX];
  char          tmpname [MKC_VNAME_MAX];
  mkc_profidx_t opidx;
  char          *rbuff;
  size_t        rsz;
  size_t        retsz;
  int           rc;

  opidx = mkc_profile_get_active (check->profiles);

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
  mkc_check_append_arg (check, "--exists");
  mkc_check_append_arg (check, pkg);
// ### --with-path
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
  mkc_check_append_arg (check, "--cflags");
  mkc_check_append_arg (check, pkg);
// ### --with-path
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
    mkc_strtrim (rbuff, retsz);
    snprintf (tmpname, sizeof (tmpname), "%s_CFLAGS", pkg);
    mkc_pvar_set_str (check->pvar, tmpname, rbuff, MKC_VCTXT_CHECK);
  }

  mkc_chk_reset (check);
  check->targc = 0;
  mkc_check_append_arg (check, pkgconfpath);
  mkc_check_append_arg (check, "--libs");
  mkc_check_append_arg (check, pkg);
// ### --with-path
  mkc_check_append_arg (check, NULL);
  if (mkc_error_chk_err (check->mkcerr)) {
    free (rbuff);
    mkc_chk_reset (check);
    return MKC_ERR_FAILURE;
  }

  rsz = MKC_SMALL_BUFF_SZ;
  rbuff = malloc (rsz);
  mkc_check_log_command (check, "pkg-libs");

  rc = mkc_os_process_pipe (check->targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);
  mkc_log (check->log, MKC_LOG_CHECK, "pkg libs: %s\n", rbuff);
  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (retsz > 0) {
    mkc_strtrim (rbuff, retsz);
    snprintf (tmpname, sizeof (tmpname), "%s_LIBS", pkg);
    mkc_pvar_set_str (check->pvar, tmpname, rbuff, MKC_VCTXT_CHECK);
  }

  mkc_pvar_profile_set_idx (check->pvar, opidx);

  mkc_chk_reset (check);
  return rc;
}

#if _package_libpkgconf

static int
mkc_chk_package_lib (mkc_check_t *check, const char *pkg)
{
  pkgconf_client_t    *pconfclient = NULL;
  pkgconf_pkg_t       *pconfpkg = NULL;
  pkgconf_fragment_t  pconflist;
  char                tmpname [MKC_VNAME_MAX];
  char                *tmp;
  mkc_profidx_t       opidx;
  int                 rc;

  pconfclient = pkgconf_client_new (NULL, NULL, NULL);
  if (pconfclient == NULL) {
    return mkc_chk_package_exec (check, pkg);
  }

  opidx = mkc_profile_get_active (check->profiles);

  pconfpkg = pkgconf_pkg_find (pconfclient, pkg);
  if (pconfpkg == NULL) {
    pkgconf_client_free (pclient);
    return MKC_ERR_FAILURE;
  }

  rc = pkgconf_pkg_cflags (pconfclient, pconfpkg, &pconflist, -1);
  if (rc != PKGCONF_PKG_ERRF_OK) {
    pkgconf_client_free (pclient);
    return MKC_ERR_FAILURE;
  }

  tmp = pkgconf_fragment_render (&pconflist);
  snprintf (tmpname, sizeof (tmpname), "%s_CFLAGS", pkg);
  mkc_pvar_set_str (check->pvar, tmpname, tmp, MKC_VCTXT_CHECK);
  pkgconf_fragment_free (&pconflist);
  free (tmp);

  rc = pkgconf_pkg_libs (pconfclient, pconfpkg, &pconflist, -1);
  if (rc != PKGCONF_PKG_ERRF_OK) {
    pkgconf_client_free (pclient);
    return MKC_ERR_FAILURE;
  }

  tmp = pkgconf_fragment_render (&pconflist);
  snprintf (tmpname, sizeof (tmpname), "%s_LIBS", pkg);
  mkc_pvar_set_str (check->pvar, tmpname, rbuff, MKC_VCTXT_CHECK);
  pkgconf_fragment_free (&pconflist);
  free (tmp);

  pkgconf_client_free (pclient);
  mkc_pvar_profile_set_idx (check->pvar, opidx);
  mkc_chk_reset (check);
  return rc;
}

#endif /* _package_libpkgconf */
