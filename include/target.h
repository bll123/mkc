/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_TARGET_H
#define INC_TARGET_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "chararr.h"
#include "comptest.h"
#include "attribute.h"
#include "mkc_compiler.h"
#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_log.h"
#include "mkc_regex.h"
#include "scopedvar.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  TARGET_NONE           = 0x0000,
  TARGET_IGNORE_SYS_INC = 0x0001,
  TARGET_SUPPORTS_MM    = 0x0002,
} target_flag_t;

enum {
  TARGET_CURRENT,
  TARGET_OUT_OF_DATE,
};

typedef struct target_t target_t;

target_t * target_init (scopedvar_t *scopedvar, comptest_t *comptest, mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr);
void target_free (target_t *target);
chararr_t *target_get_flags (target_t *target, const char *flagname);

int target_check_dependency_timestamp (target_t *target, const char *filename, const char *filepath);
void target_get_dependencies (target_t *target, mkc_compiler_t compiler, const char *filename, const char *filepath, target_flag_t flags);
void target_object_file (target_t *target, const char *execnm, const char *objnm);
void target_source_file (target_t *target, const char *objnm, const char *srcname);

mkc_list_t * target_get_include_list (target_t *target, mkc_regex_t *rx, int64_t *ts);
const char * target_iter_includes (target_t *target, mkc_list_t *hlist, mkc_listidx_t *hiteridx, char *hdr, size_t hsz);

void target_iter_dependency_ts_start (target_t *target, const char *filename, mkc_listidx_t *iteridx);
int64_t target_iter_dependency_ts (target_t *target, const char *filename, mkc_listidx_t *iteridx);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_TARGET_H */

