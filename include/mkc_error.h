/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_ERROR_H
#define INC_MKC_ERROR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  MKC_ERR_FAILURE = -1,
  MKC_OK = 0,
  MKC_OK_CHANGE,
  MKC_ERR_COMPILER_FAILURE,
  MKC_ERR_DIR_NOT_A_DIR,
  MKC_ERR_DIR_NOT_CREATED,
  MKC_ERR_DIR_UNABLE_TO_OPEN,
  MKC_ERR_DIVIDE_BY_ZERO,
  MKC_ERR_EXCEEDS_STACK_SIZE,
  MKC_ERR_FILE_NOT_FOUND,
  MKC_ERR_FILE_READ_ERROR,
  MKC_ERR_FILE_WRITE_ERROR,
  MKC_ERR_INVALID_ARGUMENT,
  MKC_ERR_INVALID_OP,
  MKC_ERR_INVALID_PROFILE,
  MKC_ERR_INVALID_VALUE,
  MKC_ERR_MISMATCHED_ARGUMENT,
  MKC_ERR_NULL_ARGUMENT,
  MKC_ERR_OUT_OF_MEMORY,
  MKC_ERR_OUT_OF_RANGE,
  MKC_ERR_PARSE_FAILURE,
  MKC_ERR_PKGCONF_NOT_FOUND,
  /* processing errors */
  MKC_ERR_PROC_INVALID_MARK,
  MKC_ERR_PROC_INVALID_METHOD,
  MKC_ERR_PROC_NO_INPUT,
  MKC_ERR_PROC_NO_METHOD,
  MKC_ERR_PROC_NO_NAME,
  MKC_ERR_PROC_NO_OUTPUT,
  MKC_ERR_REGEX_PATTERN_FAIL,
  MKC_ERR_SEARCH_UNSORTED_LIST,
  MKC_ERR_STMT_NOT_ALLOWED,
  MKC_ERR_UNBALANCED_BRACES,
  /* a serious internal error */
  MKC_ERR_UNHANDLED_VALUE,
  MKC_ERR_UNKNOWN_VARIABLE,
  MKC_ERR_USER_EXIT,
  MKC_ERR_WHILE_LIMIT_EXCEEDED,
} mkc_err_code_t;

typedef struct mkc_error_t mkc_error_t;

MKC_NODISCARD mkc_error_t *mkc_error_init (void);
void mkc_error_free (mkc_error_t *mkcerr);
void mkc_error_set (mkc_error_t *mkcerr, mkc_err_code_t err, int32_t syserr, const char *str);
void mkc_error_set_line_col (mkc_error_t *mkcerr, int32_t lineno, int colno);
bool mkc_error_chk_err (mkc_error_t *mkcerr);
bool mkc_error_chk_ok (mkc_error_t *mkcerr);
void mkc_error_print (mkc_error_t *mkcerr);
char * mkc_error_line_disp (char *buff, size_t sz, int32_t lineno, int colno);
mkc_err_code_t mkc_error_value (mkc_error_t *mkcerr);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_ERROR_H */
