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

#include "mkc_error.h"

static const char * const mkcerrormsg [] = {
  [MKC_OK] = "success",
  [MKC_OK_CHANGE] = "success, cache invalidated",
  [MKC_ERR_COMPILER_FAILURE] = "compiler failure",
  /* exceeds the set size of internal stacks */
  [MKC_ERR_EXCEEDS_STACK_SIZE] = "exceeds stack size",
  [MKC_ERR_FILE_NOT_FOUND] = "file not found",
  [MKC_ERR_INVALID_ARGUMENT] = "invalid argument",
  [MKC_ERR_INVALID_PROFILE] = "invalid profile",
  [MKC_ERR_INVALID_VALUE] = "invalid value",
  [MKC_ERR_MISMATCHED_ARGUMENT] = "mismatched argument",
  [MKC_ERR_NULL_ARGUMENT] = "null argument",
  [MKC_ERR_OUT_OF_MEMORY] = "out of memory",
  [MKC_ERR_OUT_OF_RANGE] = "out of range",
  [MKC_ERR_PARSE_FAILURE] = "parse failure",
  [MKC_ERR_PROC_NO_INPUT] = "no input",
  [MKC_ERR_PROC_NO_METHOD] = "no method",
  [MKC_ERR_PROC_NO_NAME] = "no name",
  [MKC_ERR_PROC_NO_OUTPUT] = "no output",
  [MKC_ERR_SEARCH_UNSORTED_LIST] = "searching an unsorted list",
  [MKC_ERR_STMT_NOT_ALLOWED] = "statement not allowed",
  [MKC_ERR_UNBALANCED_BRACES] = "unbalanced braces",
  /* unhandled-value is a serious internal error */
  [MKC_ERR_UNHANDLED_VALUE] = "unhandled value",
  [MKC_ERR_WHILE_LIMIT_EXCEEDED] = "while limit exceeded",
};

typedef struct mkc_error_t {
  mkc_err_code_t    err;
  int32_t             lineno;
  int                 colno;
} mkc_error_t;

mkc_error_t *
mkc_error_init (void)
{
  mkc_error_t   *mkcerr;

  mkcerr = malloc (sizeof (mkc_error_t));
  if (mkcerr == NULL) {
    return NULL;
  }
  mkcerr->err = MKC_OK;
  mkcerr->lineno = 0;
  mkcerr->colno = 0;

  return mkcerr;
}

void
mkc_error_free (mkc_error_t *mkcerr)
{
  if (mkcerr == NULL) {
    return;
  }

  free (mkcerr);
}

void
mkc_error_set (mkc_error_t *mkcerr, mkc_err_code_t err)
{
  if (mkcerr == NULL) {
    return;
  }

  mkcerr->err = err;
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
  if (mkcerr->err == MKC_OK || mkcerr->err == MKC_OK_CHANGE) {
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
  return false;
}

void
mkc_error_print (mkc_error_t *mkcerr)
{
  if (mkcerr->err == MKC_OK) {
    return;
  }
  if (mkcerr->lineno != 0) {
    char    tbuff [40];

    mkc_error_line_disp (tbuff, sizeof (tbuff), mkcerr->lineno, mkcerr->colno);
    fprintf (stderr, "%s", tbuff);
  }
  fprintf (stderr, "error: %s\n", mkcerrormsg [mkcerr->err]);
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
  if (mkcerr == NULL) {
    return MKC_ERR_INVALID_ARGUMENT;
  }

  return mkcerr->err;
}
