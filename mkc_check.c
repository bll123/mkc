/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mkc_check.h"
#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_fileop.h"
#include "mkc_log.h"
#include "mkc_pvar.h"
#include "mkc_os_process.h"
#include "mkc_string.h"

const char * const mkctestcompflags = "MKC_TV_TEST_COMP_FLAGS";

static const char *MKC_INCLUDE_PATH = ".";

typedef struct mkc_check_t {
  mkc_profile_t   * profiles;
  mkc_pvar_t      * pvar;
  mkc_error_t     * mkcerr;
  mkc_log_t       * log;
} mkc_check_t;

static void mkc_check_file_sub_copy (mkc_check_t *check, char *tbuff, size_t sz, const char *fname, const char *sfx);
static void mkc_check_log_command (mkc_check_t *check, const char *targv []);
static mkc_err_code_t mkc_chk_env_var_set (mkc_check_t *check, const char *nm);

mkc_check_t *
mkc_check_init (mkc_profile_t *profiles, mkc_pvar_t *pvar, mkc_log_t *log, mkc_error_t *mkcerr)
{
  mkc_check_t   *check;

  check = malloc (sizeof (mkc_check_t));
  check->profiles = profiles;
  check->pvar = pvar;
  check->mkcerr = mkcerr;
  check->log = log;

  return check;
}

void
mkc_check_free (mkc_check_t *check)
{
  if (check == NULL) {
    return;
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
mkc_chk_compiler_works (mkc_check_t *check,
    const char *compiler, const char *sfx)
{
  int         rc;
  const char  *flags [2];

  /* clang prints the deprecated error when compiling C with */
  /* c++ or objective-c */
  /* 2026-5-29 when this is run, the compiler is not yet known */
  flags [0] = "-Wno-deprecated";
  flags [1] = NULL;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: compiler-works\n");
  rc = mkc_compile_only (check, compiler, sfx,
      "int-main", NULL, flags);
  return rc;
}

int
mkc_chk_header_modern (mkc_check_t *check,
    const char *compiler, const char *sfx)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: header-modern\n");
  rc = mkc_compile_only (check, compiler, sfx,
        "int-header-modern", MKC_INCLUDE_PATH, NULL);
  return rc;
}

int
mkc_chk_system_type (mkc_check_t *check,
    const char *compiler, const char *sfx)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: system-type\n");
  rc = mkc_compile_run (check, compiler, sfx,
      "int-system", MKC_INCLUDE_PATH, NULL, NULL, 0);
  return rc;
}

int
mkc_chk_system_id (mkc_check_t *check,
    const char *compiler, const char *sfx)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: system-id\n");
  rc = mkc_compile_run (check, compiler, sfx,
      "int-sysid", MKC_INCLUDE_PATH, NULL, NULL, 0);
  return rc;
}

int
mkc_chk_variadic_macro (mkc_check_t *check,
    const char *compiler, const char *sfx)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: variadic-macro\n");
  rc = mkc_compile_only (check, compiler, sfx,
        "int-variadic-macro", NULL, NULL);
  return rc;
}

/* the library location is used for linux systems */
/* some linux systems use lib64 as the main library suffix */
/* other linux systems have lib64, but only use it for lib64 specific */
/* libraries */
int
mkc_chk_library_location (mkc_check_t *check,
    const char *compiler, const char *sfx)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: lib-location\n");
  rc = mkc_compile_run (check, compiler, sfx,
      "int-libloc", MKC_INCLUDE_PATH, NULL, NULL, 0);
  return rc;
}

int
mkc_chk_which_compiler (mkc_check_t *check,
    const char *compiler, const char *sfx)
{
  int         rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: which-compiler\n");
  rc = mkc_compile_run (check, compiler, sfx,
      "int-compiler", MKC_INCLUDE_PATH, NULL, NULL, 0);
  return rc;
}

const char *
mkc_chk_guess_suffix (const char *ccstr)
{
  const char  *sfx = ".c";

  if (strstr (ccstr, "++") != NULL) {
    sfx = ".cpp";
  }

  return sfx;
}

const char *
mkc_chk_compiler_suffix (int compiler)
{
  const char  *sfx = ".c";

  switch (compiler) {
    case MKC_COMP_C: {
      sfx = ".c";
      break;
    }
    case MKC_COMP_CXX: {
      sfx = ".cpp";
      break;
    }
    case MKC_COMP_OBJC: {
      sfx = ".m";
      break;
    }
  }

  return sfx;
}

int
mkc_chk_compiler_id (mkc_check_t *check,
    const char *compiler, const char *sfx)
{
  int       rc;

  mkc_log (check->log, MKC_LOG_CHECK, "  == chk: compiler-id\n");
  rc = mkc_compile_run (check, compiler, sfx,
      "int-compid", MKC_INCLUDE_PATH, NULL, NULL, 0);
  return rc;
}

int
mkc_chk_compiler_flag (mkc_check_t *check,
    const char *compiler, const char *sfx,
    const char *flag, int negate)
{
  int               rc;
  const char        *flags [2];
  char              tbuff [100];
  char              rbuff [400];
  static const char *negprefix = "-Wno-";
  static size_t     neglen = 5;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: compiler-flag: %s\n", flag);
  stpecpy (tbuff, tbuff + sizeof (tbuff), flag);
  if (negate == MKC_NEGATE) {
    char    *p;

    if (strncmp (flag, negprefix, neglen) == 0) {
      p = stpecpy (tbuff, tbuff + sizeof (tbuff), "-W");
      p = stpecpy (p, tbuff + sizeof (tbuff), flag + neglen);
    }
  }

  flags [0] = tbuff;
  flags [1] = NULL;

  rc = mkc_compile_link (check, compiler, sfx,
      "int-main", NULL, flags, rbuff, sizeof (rbuff));
  if (rc == 0) {
    /* clang does not return an error code on a unknown warning */
    if (strstr (rbuff, "warning") != NULL) {
      rc = 1;
    }
  }
  return rc;
}

int
mkc_chk_link_flag (mkc_check_t *check,
    const char *compiler, const char *sfx,
    const char *flag)
{
  int               rc;
  const char        *flags [2];
  char              rbuff [400];

  flags [0] = flag;
  flags [1] = NULL;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: link-flag: %s\n", flag);
  rc = mkc_compile_link (check, compiler, sfx,
      "int-main", NULL, flags, rbuff, sizeof (rbuff));
  if (rc == 0) {
    /* clang does not return an error code on a unknown warning */
    if (strstr (rbuff, "warning") != NULL) {
      rc = 1;
    }
  }
  return rc;
}

int
mkc_chk_size (mkc_check_t *check,
    const char *compiler, const char *sfx,
    const char *type)
{
  int             rc;
  mkc_profidx_t   pidx;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: size: %s\n", type);
  mkc_profile_push (check->profiles);
  mkc_pvar_profile_set (check->pvar,
      MKC_PROF_INTERNAL_NAME, MKC_PROF_COMPILER_GENERAL);

  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_SIZE", type);
  pidx = mkc_profile_pop (check->profiles);
  mkc_pvar_profile_set_idx (check->pvar, pidx);

  rc = mkc_compile_run (check, compiler, sfx,
        "c-size", NULL, NULL, NULL, 0);
  if (rc < 0) {
    rc = 0;
  }
  return rc;
}

int
mkc_chk_type (mkc_check_t *check,
    const char *compiler, const char *sfx,
    const char *type)
{
  int             rc;
  mkc_profidx_t   pidx;

  mkc_log (check->log, MKC_LOG_CHECK, "== chk: type: %s\n", type);
  mkc_profile_push (check->profiles);
  mkc_pvar_profile_set (check->pvar,
      MKC_PROF_INTERNAL_NAME, MKC_PROF_COMPILER_GENERAL);

  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_TYPE", type);
  pidx = mkc_profile_pop (check->profiles);
  mkc_pvar_profile_set_idx (check->pvar, pidx);

  rc = mkc_compile_only (check, compiler, sfx,
        "c-type", NULL, NULL);
  return rc;
}

int
mkc_chk_struct_member (mkc_check_t *check,
    const char *compiler, const char *sfx,
    const char *structname, const char *membername)
{
  int             rc;
  mkc_profidx_t   pidx;

  mkc_log (check->log, MKC_LOG_CHECK,
      "== chk: struct member: %s.%s\n", structname, membername);
  mkc_profile_push (check->profiles);
  mkc_pvar_profile_set (check->pvar,
      MKC_PROF_INTERNAL_NAME, MKC_PROF_COMPILER_GENERAL);

  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_STRUCT_NAME", structname);
  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_STRUCT_MEMBER", membername);
  pidx = mkc_profile_pop (check->profiles);
  mkc_pvar_profile_set_idx (check->pvar, pidx);

  rc = mkc_compile_only (check, compiler, sfx,
        "c-struct-member", NULL, NULL);
  return rc;
}

int
mkc_chk_function (mkc_check_t *check, const char *compiler, const char *sfx,
    const char *funcname)
{
  int             rc;
  mkc_profidx_t   pidx;

  mkc_log (check->log, MKC_LOG_CHECK,
      "== chk: function: %s\n", funcname);
  mkc_profile_push (check->profiles);
  mkc_pvar_profile_set (check->pvar,
      MKC_PROF_INTERNAL_NAME, MKC_PROF_COMPILER_GENERAL);

  mkc_pvar_set_str (check->pvar, "MKC_TV_TEST_FUNCTION_NAME", funcname);
  pidx = mkc_profile_pop (check->profiles);
  mkc_pvar_profile_set_idx (check->pvar, pidx);

  rc = mkc_compile_only (check, compiler, sfx,
        "c-function", NULL, NULL);
  return rc;
}


int
mkc_compile_only (mkc_check_t *check,
    const char *compiler, const char *sfx,
    const char *fname,
    const char *incpath, const char *flags [])
{
  const char  *targv [30];
  int         targc = 0;
  int         rc;
  char        tbuff [MKC_PATH_MAX];
  char        *rbuff;
  size_t      rsz = 4000;
  size_t      retsz;

  mkc_check_file_sub_copy (check, tbuff, sizeof (tbuff), fname, sfx);

  targv [targc++] = compiler;
  if (flags != NULL) {
    int         fcount = 0;

    while (flags [fcount] != NULL) {
      targv [targc++] = flags [fcount];
      ++fcount;
    }
  }
  if (incpath != NULL) {
    targv [targc++] = "-I";
    targv [targc++] = incpath;
  }
  targv [targc++] = "-c";
  targv [targc++] = "-o";
  targv [targc++] = "mkc_files/tmp/mkctest.o";
  targv [targc++] = tbuff;
  targv [targc++] = NULL;

  mkc_check_log_command (check, targv);

  rbuff = malloc (rsz);
  if (rbuff == NULL) {
    mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY);
    return MKC_ERR_FAILURE;
  }

  rc = mkc_os_process_pipe (targv,
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

  free (rbuff);

  return rc;
}

int
mkc_compile_link (mkc_check_t *check,
    const char *compiler, const char *sfx,
    const char *fname, const char *incpath, const char *flags [],
    char *rbuff, size_t rsz)
{
  const char  *targv [30];
  int         targc = 0;
  int         rc;
  char        tbuff [MKC_PATH_MAX];
  size_t      retsz;
  bool        rallocated = false;

  mkc_check_file_sub_copy (check, tbuff, sizeof (tbuff), fname, sfx);

  targv [targc++] = compiler;
  if (flags != NULL) {
    int         fcount = 0;

    while (flags [fcount] != NULL) {
      targv [targc++] = flags [fcount];
      ++fcount;
    }
  }
  if (incpath != NULL) {
    targv [targc++] = "-I";
    targv [targc++] = incpath;
  }
  targv [targc++] = "-o";
  targv [targc++] = "mkc_files/tmp/mkctest.exe";
  targv [targc++] = tbuff;
  targv [targc++] = NULL;

  mkc_check_log_command (check, targv);

  if (rbuff == NULL) {
    rsz = 4000;
    rbuff = malloc (rsz);
    if (rbuff == NULL) {
      mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY);
      return MKC_ERR_FAILURE;
    }
    rallocated = true;
  }

  rc = mkc_os_process_pipe (targv,
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);

  mkc_log (check->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (retsz > 0) {
    mkc_log (check->log, MKC_LOG_CHECK, "--- compile log\n");
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
    const char *compiler, const char *sfx,
    const char *fname, const char *incpath, const char *flags [],
    char *rbuff, size_t rsz)
{
  const char  *targv [10];
  int         targc = 0;
  int         rc;
  bool        rallocated = false;
  size_t      retsz;

  rc = mkc_compile_link (check, compiler, sfx,
      fname, incpath, flags, NULL, 0);

  if (rc != 0) {
    return rc;
  }

  targc = 0;
  targv [targc++] = "mkc_files/tmp/mkctest.exe";
  targv [targc++] = NULL;

  mkc_check_log_command (check, targv);

  if (rbuff == NULL) {
    rsz = 4000;
    rbuff = malloc (rsz);
    if (rbuff == NULL) {
      mkc_error_set (check->mkcerr, MKC_ERR_OUT_OF_MEMORY);
      return MKC_ERR_FAILURE;
    }
    rallocated = true;
  }

  rc = mkc_os_process_pipe (targv,
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
mkc_check_log_command (mkc_check_t *check, const char *targv [])
{
  int   targc;

  targc = 0;
  mkc_log (check->log, MKC_LOG_CHECK, "cmd: ");
  while (targv [targc] != NULL) {
    mkc_log (check->log, MKC_LOG_CHECK, "%s ", targv [targc]);
    ++targc;
  }
  mkc_log (check->log, MKC_LOG_CHECK, "\n");
}

static mkc_err_code_t
mkc_chk_env_var_set (mkc_check_t *check, const char *nm)
{
  char            tbuff [MKC_PATH_MAX];
  mkc_err_code_t  rc = MKC_OK;

  *tbuff = '\0';
  mkc_env_get (nm, tbuff, sizeof (tbuff));
  if (*tbuff) {
    rc = mkc_pvar_set_str (check->pvar, nm, tbuff);
  }

  return rc;
}
