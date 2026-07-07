/*
 * Copyright 2026 Brad Lanam Pleasant Hill, CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "mkc_error.h"
#include "mkc_string.h"

static char const * const mkcerrormsg [] = {
  [MKC_OK] = "success",
  [MKC_OK_CHANGE] = "success, cache invalidated",
  [MKC_ERR_COMPILER_FAILURE] = "compiler failure",
  [MKC_ERR_DIR_NOT_A_DIR] = "not a directory",
  [MKC_ERR_DIR_NOT_CREATED] = "directory could not be created",
  [MKC_ERR_DIR_UNABLE_TO_OPEN] = "directory could not be opened",
  [MKC_ERR_DIVIDE_BY_ZERO] = "division by zero",
  [MKC_ERR_EXCEEDS_STACK_SIZE] = "exceeds stack size",
  [MKC_ERR_FATAL_ERROR] = "fatal error",
  [MKC_ERR_FILE_NOT_FOUND] = "file not found",
  [MKC_ERR_FILE_READ_ERROR] = "file read error",
  [MKC_ERR_FILE_WRITE_ERROR] = "file write error",
  [MKC_ERR_FUNCTION_ARG_MISMATCH] = "function argument mismatch",
  [MKC_ERR_FUNCTION_NOT_FOUND] = "function not found",
  [MKC_ERR_INVALID_ARGUMENT] = "invalid argument",
  [MKC_ERR_INVALID_OP] = "invalid operation",
  [MKC_ERR_INVALID_PROFILE] = "invalid profile",
  [MKC_ERR_INVALID_VALUE] = "invalid value",
  [MKC_ERR_LOOP_LIMIT_EXCEEDED] = "loop limit exceeded",
  [MKC_ERR_MISMATCHED_ARGUMENT] = "mismatched argument",
  [MKC_ERR_NULL_ARGUMENT] = "null argument",
  [MKC_ERR_OUT_OF_MEMORY] = "out of memory",
  [MKC_ERR_OUT_OF_RANGE] = "out of range",
  [MKC_ERR_PARSE_FAILURE] = "parse failure",
  [MKC_ERR_PKGCONF_NOT_FOUND] = "pkgconf not found",
  [MKC_ERR_PROC_INVALID_MARK] = "invalid mark",
  [MKC_ERR_PROC_INVALID_METHOD] = "invalid method",
  [MKC_ERR_PROC_NO_INPUT] = "no input",
  [MKC_ERR_PROC_NO_METHOD] = "no method",
  [MKC_ERR_PROC_NO_NAME] = "no name",
  [MKC_ERR_PROC_NO_OUTPUT] = "no output",
  [MKC_ERR_REGEX_MATCH_FAIL] = "regex match failed",
  [MKC_ERR_REGEX_PATTERN_FAIL] = "regex pattern failed to compile",
  [MKC_ERR_SEARCH_UNSORTED_LIST] = "searching an unsorted list",
  [MKC_ERR_STMT_NOT_ALLOWED] = "statement not allowed",
  [MKC_ERR_UNBALANCED_BRACES] = "unbalanced braces",
  /* unhandled-value is a serious internal error */
  [MKC_ERR_UNHANDLED_VALUE] = "unhandled value",
  [MKC_ERR_UNKNOWN_VARIABLE] = "unknown variable",
  [MKC_ERR_USER_EXIT] = "user exit",
};

typedef struct mkc_error_t {
  char            *str;
  const char      *func;
  int32_t         flineno;
  int32_t         lineno;
  int32_t         syserr;
  int             colno;
  mkc_err_code_t  err;
} mkc_error_t;

MKC_NODISCARD
mkc_error_t *
mkc_error_init (void)
{
  mkc_error_t   *mkcerr;

  mkcerr = malloc (sizeof (mkc_error_t));
  if (mkcerr == NULL) {
    return NULL;
  }
  mkcerr->str = NULL;
  mkcerr->func = NULL;
  mkcerr->err = MKC_OK;
  mkcerr->syserr = 0;
  mkcerr->lineno = 0;
  mkcerr->flineno = 0;
  mkcerr->colno = 0;

  return mkcerr;
}

void
mkc_error_free (mkc_error_t *mkcerr)
{
  if (mkcerr == NULL) {
    return;
  }

  datafree (mkcerr->str);
  free (mkcerr);
}

void
r_mkc_error_set (mkc_error_t *mkcerr, mkc_err_code_t err,
    int syserr, const char *str, const char *func, int flineno)
{
  if (mkcerr == NULL) {
    return;
  }

  mkcerr->err = err;
  mkcerr->syserr = syserr;
  datafree (mkcerr->str);
  mkcerr->str = NULL;
  mkcerr->func = func;
  mkcerr->flineno = flineno;
  if (str != NULL) {
    mkcerr->str = strdup (str);
  }
}

void
mkc_error_set_line_col (mkc_error_t *mkcerr, int32_t lineno, int colno)
{
  if (mkcerr == NULL) {
    return;
  }

  mkcerr->lineno = lineno;
  mkcerr->colno = colno;
}

bool
mkc_error_chk_err (mkc_error_t *mkcerr)
{
  if (mkcerr == NULL) {
    return true;
  }
  if (mkcerr->err == MKC_OK ||
      mkcerr->err == MKC_OK_CHANGE) {
    return false;
  }
  if (mkcerr->err == MKC_ERR_USER_EXIT && mkcerr->syserr == 0) {
    return false;
  }
  return true;
}

bool
mkc_error_chk_ok (mkc_error_t *mkcerr)
{
  if (mkcerr == NULL) {
    return false;
  }
  if (mkcerr->err == MKC_OK || mkcerr->err == MKC_OK_CHANGE) {
    return true;
  }
  if (mkcerr->err == MKC_ERR_USER_EXIT && mkcerr->syserr == 0) {
    return true;
  }
  return false;
}

void
mkc_error_print (mkc_error_t *mkcerr)
{
  if (mkcerr->err == MKC_OK) {
    return;
  }
  if (mkcerr->err == MKC_ERR_USER_EXIT && mkcerr->syserr == 0) {
    fprintf (stderr, "-- %s\n", mkcerrormsg [mkcerr->err]);
    return;
  }

  if (mkcerr->lineno != 0) {
    char    tbuff [40];

    mkc_error_line_disp (tbuff, sizeof (tbuff), mkcerr->lineno, mkcerr->colno);
    fprintf (stderr, "%s", tbuff);
  }
  fprintf (stderr, "error: %s", mkcerrormsg [mkcerr->err]);
  if (mkcerr->err == MKC_ERR_USER_EXIT) {
    fprintf (stderr, " (%d)", mkcerr->syserr);
  }
  if (mkcerr->err != MKC_ERR_USER_EXIT &&
      mkcerr->syserr != 0) {
    fprintf (stderr, "; %s", strerror (mkcerr->syserr));
  }
  if (mkcerr->str != NULL) {
    fprintf (stderr, "; %s", mkcerr->str);
  }
  if (mkcerr->func != NULL) {
    fprintf (stderr, "; %s:%d", mkcerr->func, mkcerr->flineno);
  }

  fprintf (stderr, "\n");
}

char *
mkc_error_line_disp (char *buff, size_t sz, int32_t lineno, int colno)
{
  if (buff == NULL) {
    return NULL;
  }

  snprintf (buff, sz, "%" PRId32 ":%d: ", lineno, colno);

  return buff;
}

mkc_err_code_t
mkc_error_value (mkc_error_t *mkcerr)
{
  int     rc;

  if (mkcerr == NULL) {
    return MKC_ERR_INVALID_ARGUMENT;
  }
  rc = mkcerr->err;
  if (rc == MKC_ERR_USER_EXIT) {
    rc = mkcerr->syserr;
  }
  return rc;
}
