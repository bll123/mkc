/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_TARGET_H
#define INC_TARGET_H

#include <stddef.h>
#include <stdbool.h>

#include "chararr.h"
#include "comptest.h"
#include "attribute.h"
#include "mkc_compiler.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "scopedvar.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct target_t target_t;

target_t * target_init (scopedvar_t *scopedvar, comptest_t *comptest, mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr);
void target_free (target_t *target);
chararr_t *target_get_flags (target_t *target, const char *flagname);
void target_get_dependencies (target_t *target, mkc_compiler_t compiler, const char *filename, const char *filepath);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_TARGET_H */

