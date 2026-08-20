/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_COMPILE_H
#define INC_COMPILE_H

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
  COMPILE_COMPILE,
  COMPILE_COMPILE_LINK,
  COMPILE_COMPILE_LINK_RUN,
  COMPILE_LINK,
} ct_type_t;

typedef struct compile_t compile_t;

compile_t * compile_init (scopedvar_t *scopedvar, mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr);
void compile_free (compile_t *compile);
void compile_set_flags (compile_t *compile, chararr_t *compflags, chararr_t *ldflags, chararr_t *libs);
void compile_set_compiler (compile_t *compile, mkc_compiler_t compiler);
void compile_set_output (compile_t *compile, const char *outpath);
void compile_preprocess (compile_t *compile);
void compile_usetemplate (compile_t *compile);
void compile_reset (compile_t *compile);
void compile_append_object (compile_t *compile, const char *objpath);
void compile_append_compflag (compile_t *compile, const char *flag);
void compile_append_linkflag (compile_t *compile, const char *flag);

void compile_create_header_var (compile_t *compile);
int compile_exec (compile_t *compile, ct_type_t ctype, mkc_compiler_t compiler, const char *fname, char *rbuff, size_t rsz);
const char * compile_get_compstr (compile_t *compile, mkc_compiler_t compiler, char *buff, size_t sz);
void compile_file_sub_copy (compile_t *compile, char *tbuff, size_t sz, const char *fname, const char *origsfx, const char *sfx);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_COMPILE_H */
