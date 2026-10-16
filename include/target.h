/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_TARGET_H
#define INC_TARGET_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "attribute.h"
#include "chararr.h"
#include "compile.h"
#include "list.h"
#include "mkc_compiler.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"
#include "mkc_regex.h"
#include "scopedvar.h"
#include "toposort.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  TARGET_NONE           = 0x0000,
  TARGET_IGNORE_SYS_INC = 0x0001,
  TARGET_USE_MM         = 0x0002,
} target_flag_t;

enum {
  TARGET_CURRENT,
  TARGET_OUT_OF_DATE,
};

/* use values that will not change when new target types are added */
typedef enum {
  TGT_T_EXEC = 'e',
  TGT_T_INCLUDE = 'i',
  TGT_T_LIBRARY = 'l',
  TGT_T_OBJECT = 'o',
  /* source is any source file that already exists */
  TGT_T_SOURCE = 's',
  TGT_T_USER_EXEC = 'E',
  TGT_T_USER_FILE = 'F',
  TGT_T_USER_LIBRARY = 'L',
  TGT_T_USER_OBJECT = 'O',
} tgt_type_t;

typedef struct target_t target_t;

MKC_NODISCARD target_t * target_init (scopedvar_t *scopedvar, compile_t *compile, mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr);
void target_free (target_t *target);
chararr_t *target_get_flags (target_t *target, const char *flagname, chararr_t *include_paths);
void target_set_changed (target_t *target, bool changed);

void target_topo_add_items (target_t *target, toposort_t *topo, list_t *hlist);
void target_topo_add_deps (target_t *target, toposort_t *topo, const char *filename, sv_type_t svtype);

int target_check_dependency_timestamp (target_t *target, const char *filepath, sv_type_t svtype);
void target_get_dependencies (target_t *target, mkc_compiler_t compiler, const char *tgtname, const char *filepath, target_flag_t flags, chararr_t *cflags, sv_type_t svtype);
void target_create_target (target_t *target, const char *path, tgt_type_t tgttype, const char *srcpath);
void target_create_dependency (target_t *target, const char *path, const char *deppath);
void target_object_source (target_t *target, const char *objnm, const char *srcname);
int target_config_file (target_t *target, const char *outfile, const char *mkcfile, const char *infile);
void target_build (target_t *target, list_t *blist);

MKC_NODISCARD list_t * target_get_include_list (target_t *target, chararr_t * include_paths, mkc_regex_t *rx, int64_t *ts);
const char * target_iter_includes (target_t *target, list_t *hlist, listidx_t *hiteridx, char *hdr, size_t hsz);

void target_iter_dependency_ts_start (target_t *target, const char *filename, listidx_t *iteridx, sv_type_t svtype);
int64_t target_iter_dependency_ts (target_t *target, const char *filename, listidx_t *iteridx, sv_type_t svtype);

const char * target_disp_type (tgt_type_t tgttype);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_TARGET_H */

