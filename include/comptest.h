/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_COMPTEST_H
#define INC_COMPTEST_H

#include <stddef.h>
#include <stdbool.h>

#include "attribute.h"
#include "chararr.h"
#include "mkc_compiler.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "scopedvar.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  MKC_COMPILE_ONLY,
  MKC_COMPILE_LINK,
  MKC_COMPILE_RUN,
} ct_type_t;

typedef struct comptest_t comptest_t;

comptest_t * comptest_init (scopedvar_t *scopedvar, mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr);
void comptest_free (comptest_t *comptest);
void comptest_set_flags (comptest_t *comptest, chararr_t *compflags, chararr_t *ldflags, chararr_t *libs);
void comptest_set_compiler (comptest_t *comptest, mkc_compiler_t compiler);
void comptest_preprocess (comptest_t *comptest);
void comptest_usetemplate (comptest_t *comptest);
void comptest_reset (comptest_t *comptest);

void comptest_create_header_var (comptest_t *comptest);
int comptest_test (comptest_t *comptest, ct_type_t ctype, mkc_compiler_t compiler, const char *fname, char *rbuff, size_t rsz);
const char * comptest_get_compstr (comptest_t *comptest, mkc_compiler_t compiler, char *buff, size_t sz);
void comptest_file_sub_copy (comptest_t *comptest, char *tbuff, size_t sz, const char *fname, const char *origsfx, const char *sfx);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_COMPTEST_H */
