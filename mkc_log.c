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
#include "mkc_fileop.h"
#include "mkc_log.h"

typedef struct mkc_log_t {
  FILE        * fh;
  char        * fname;
  mkc_error_t * mkcerr;
  int32_t     logflag;
} mkc_log_t;

const char * const mkcerrormsg [] = {
  [MKC_OK] = "success",
  [MKC_ERR_COMPILER_FAILURE] = "compiler failure",
  [MKC_ERR_FILE_NOT_FOUND] = "file not found",
  [MKC_ERR_INVALID_ARGUMENT] = "invalid argument",
  [MKC_ERR_INVALID_PROFILE] = "invalid profile",
  [MKC_ERR_INVALID_VALUE] = "invalid value",
  [MKC_ERR_MISMATCHED_ARGUMENT] = "mismatched argument",
  [MKC_ERR_OUT_OF_MEMORY] = "out of memory",
  [MKC_ERR_OUT_OF_RANGE] = "out of range",
  [MKC_ERR_PARSE_FAILURE] = "parse failure",
  [MKC_ERR_SEARCH_UNSORTED_LIST] = "searching an unsorted list",
  [MKC_ERR_STMT_NOT_ALLOWED] = "statement not allowed",
  [MKC_ERR_UNBALANCED_BRACES] = "unbalanced braces",
  [MKC_ERR_UNHANDLED_VALUE] = "unhandled value",
  [MKC_ERR_WHILE_LIMIT_EXCEEDED] = "while limit exceeded",
};

mkc_log_t *
mkc_log_init (mkc_error_t *mkcerr)
{
  mkc_log_t *log;

  log = malloc (sizeof (mkc_log_t));
  if (log == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY);
    return NULL;
  }

  log->fh = NULL;
  log->fname = NULL;
  log->mkcerr = mkcerr;

  return log;
}

void
mkc_log_open (mkc_log_t *log, const char *fname, int32_t logflag)
{
  if (log == NULL) {
    return;
  }
  if (fname == NULL) {
    mkc_error_set (log->mkcerr, MKC_ERR_INVALID_ARGUMENT);
    return;
  }

  log->fname = strdup (fname);
  if (log->fname == NULL) {
    mkc_error_set (log->mkcerr, MKC_ERR_OUT_OF_MEMORY);
    return;
  }

  log->fh = mkc_fopen (fname, "w");
  if (log->fh == NULL) {
    mkc_error_set (log->mkcerr, MKC_ERR_FILE_NOT_FOUND);
  }

  log->logflag = logflag;

  return;
}

void
mkc_log_free (mkc_log_t *log)
{
  if (log == NULL) {
    return;
  }

  if (log->fh != NULL) {
    fclose (log->fh);
  }
  if (log->fname != NULL) {
    free (log->fname);
  }

  free (log);
}

void
mkc_message (const char *fmt, ...)
{
  va_list   vap;

  va_start (vap, fmt);
  vfprintf (stderr, fmt, vap);
  va_end (vap);
}

const char *
mkc_success_msg (int rc)
{
  if (rc == 0) {
    return "success";
  } else {
    return "fail";
  }
}

const char *
mkc_elapsed_disp (time_t etm, char *buff, size_t sz)
{
  snprintf (buff, sz, "%" PRId32 "ms", (int32_t) etm);
  return buff;
}

void
mkc_log (mkc_log_t *log, int32_t logflag, const char *fmt, ...)
{
  va_list   vap;

  if (log == NULL) {
    return;
  }
  if (log->fh == NULL) {
    return;
  }

  if ((logflag & log->logflag) == 0) {
    return;
  }

  va_start (vap, fmt);
  vfprintf (log->fh, fmt, vap);
  va_end (vap);
}

void
mkc_log_loc (mkc_log_t *log, int32_t logflag,
    int32_t lineno, int col, const char *fmt, ...)
{
  va_list   vap;
  char      tbuff [40];

  if (log == NULL) {
    return;
  }
  if (log->fh == NULL) {
    return;
  }

  if ((logflag & log->logflag) == 0) {
    return;
  }

  va_start (vap, fmt);
  mkc_error_line_disp (tbuff, sizeof (tbuff), lineno, col);
  fprintf (log->fh, "%s", tbuff);
  vfprintf (log->fh, fmt, vap);
  va_end (vap);
}

