/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#pragma once

#include <stdint.h>
#include <time.h>

#include "mkc_error.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

enum {
  MKC_LOG_NONE        = 0,
  MKC_LOG_AST         = (1 << 0),
  MKC_LOG_AST_PROCESS = (1 << 1),
  MKC_LOG_PROCESS     = (1 << 2),
  MKC_LOG_PROFILE     = (1 << 3),
  MKC_LOG_CHECK       = (1 << 4),
  MKC_LOG_VAR         = (1 << 5),
  MKC_LOG_STATISTICS  = (1 << 6),
  MKC_LOG_GENERAL     = (1 << 7),
  MKC_LOG_NORMAL      = (MKC_LOG_AST_PROCESS |
                         MKC_LOG_PROFILE |
                         MKC_LOG_CHECK |
                         MKC_LOG_GENERAL |
                         MKC_LOG_STATISTICS),
  MKC_LOG_ALL         = (~ 0),
};

typedef struct mkc_log_t mkc_log_t;

mkc_log_t * mkc_log_init (mkc_error_t *mkcerr);
void mkc_log_open (mkc_log_t *log, const char *fname, int32_t logflag);
void mkc_log_free (mkc_log_t *log);

void mkc_message (const char *fmt, ...);
const char * mkc_success_msg (int rc);
const char * mkc_elapsed_disp (time_t etm, char *buff, size_t sz);
void mkc_log (mkc_log_t *log, int32_t logflag, const char *fmt, ...);
void mkc_log_loc (mkc_log_t *log, int32_t logflag, int32_t lineno, int col, const char *fmt, ...);
void mkc_error_disp (mkc_error_t mkcerr);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif
