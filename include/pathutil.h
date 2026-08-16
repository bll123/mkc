/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_PATHUTIL_H
#define INC_PATHUTIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "mkc_def.h"
#include "mkc_error.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  MKC_PATH_CONFIG,              /* $HOME/.config/mkc/ or */
                                /* $USERPROFILE/AppData/Roaming/mkc */
  MKC_PATH_CWD,
  MKC_PATH_EXEC_PATH,
  MKC_PATH_EXEC_PREFIX,
  MKC_PATH_HOME,
  MKC_PATH_MKCFILES,            /* mkc_files */
  MKC_PATH_MKCF_OBJECTS,        /* mkc_files/objects */
  MKC_PATH_MKCF_STAGE,          /* mkc_files/stage */
  MKC_PATH_MKCF_TMP,            /* mkc_files/tmp */
  MKC_PATH_MKC_SHR_INCLUDE,     /* .../share/mkc/include */
  MKC_PATH_MKC_SHR_UNITS,       /* .../share/mkc/units */
  MKC_PATH_MKC_TEMPLATES,       /* .../share/mkc/templates */
  MKC_PATH_ORIG_CWD,
  MKC_PATH_PREFIX,
  MKC_PATH_SHARE,               /* .../share */
  MKC_PATH_STAGE_BIN,           /* mkcf-stage/prefix/bin */
  MKC_PATH_STAGE_INCLUDE,       /* mkcf-stage/prefix/include */
  MKC_PATH_STAGE_LIB,           /* mkcf-stage/prefix/lib */
  MKC_PATH_BUILD_MAX,
} mkc_path_t;

typedef enum {
  MKC_DIR_CWD,
  MKC_DIR_EXEC,
  MKC_DIR_EXEC_PREFIX,
  MKC_DIR_HOME,
  MKC_DIR_MKC_FILES,
  MKC_DIR_ORIG_CWD,
  MKC_DIR_PREFIX,
  MKC_DIR_SHARE,
  MKC_DIR_MAX,
} mkc_dir_t;

extern const char * const pathdesc [MKC_PATH_BUILD_MAX];

void path_build (mkc_path_t pathtype, char *buff, size_t sz, const char *filename, mkc_error_t *mkcerr);
void path_set_dir (mkc_dir_t dir, const char *path);
void path_realpath (char *path, size_t sz);
const char * path_filename (const char *path);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_PATHUTIL_H */
