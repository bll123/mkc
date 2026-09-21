/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_CHECK_H
#define INC_MKC_CHECK_H

#include <stddef.h>
#include <stdbool.h>

#include "attribute.h"
#include "chararr.h"
#include "compile.h"
#include "mkc_compiler.h"
#include "mkc_error.h"
#include "list.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"
#include "scopedvar.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct check_t check_t;

MKC_NODISCARD check_t *check_init (scopedvar_t *scopedvar, compile_t *compile, mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr);
void check_free (check_t *check);

mkc_err_code_t check_compiler_env (check_t *check);
int check_compiler_works (check_t *check, mkc_compiler_t compiler);
int check_compiler_id (check_t *check, mkc_compiler_t compiler);

void check_reset (check_t *check);

/* internal checks */
int check_header_modern (check_t *check, mkc_compiler_t compiler);
int check_library_location (check_t *check, mkc_compiler_t compiler);
int check_system_id (check_t *check, mkc_compiler_t compiler);
int check_system_type (check_t *check, mkc_compiler_t compiler);
int check_variadic_macro (check_t *check, mkc_compiler_t compiler);
int check_getconf (check_t *check);

/* user checks */
int check_arg_count (check_t *check, mkc_compiler_t compiler, const char *funcname);
int check_compiler_flag (check_t *check, mkc_compiler_t compiler, const char *flag, bool negate);
int check_const (check_t *check, mkc_compiler_t compiler, const char *consttxt);
int check_define (check_t *check, mkc_compiler_t compiler, const char *def);
int check_function (check_t *check, mkc_compiler_t compiler, const char *funcname);
int check_header (check_t *check, mkc_compiler_t compiler, const char *header, chararr_t * compflags);
int check_link_flag (check_t *check, mkc_compiler_t compiler, const char *flag);
int check_package (check_t *check, mkc_compiler_t compiler, const char *pkg);
int check_size (check_t *check, mkc_compiler_t compiler, const char *type);
int check_struct_member (check_t *check, mkc_compiler_t compiler, const char *structname, const char *membername);
int check_type (check_t *check, mkc_compiler_t compiler, const char *type);

void check_get_include_deps (check_t *check, mkc_compiler_t compiler, const char *filename, const char *filepath);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_CHECK_H */
