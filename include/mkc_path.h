/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_PATH_H
#define INC_MKC_PATH_H

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
  MKC_PATH_EXEC_PATH,
  MKC_PATH_HOME,
  MKC_PATH_MKC_FILES,           /* mkc_files */
  MKC_PATH_MKC_TMP,             /* mkc_files/tmp */
  MKC_PATH_MKC_INCLUDE,         /* .../share/mkc/include */
  MKC_PATH_MKC_TEMPLATES,       /* .../share/mkc/templates */
  MKC_PATH_MKC_UNITS,           /* .../share/mkc/units */
  MKC_PATH_MKC_USER_UNITS,      /* <config>/units */
  MKC_PATH_ORIG_CWD,
  MKC_PATH_SHARE,               /* .../share */
  MKC_PATH_PREFIX,              /* one level above exec */
  MKC_PATH_BUILD_MAX,
} mkc_path_t;

typedef enum {
  MKC_DIR_EXEC,
  MKC_DIR_HOME,
  MKC_DIR_MKC_FILES,
  MKC_DIR_SHARE,
  MKC_DIR_ORIG_CWD,
  MKC_DIR_PREFIX,           /* one level above exec */
  MKC_DIR_MAX,
} mkc_dir_t;

extern const char * const pathdesc [MKC_PATH_BUILD_MAX];

void mkc_path_build (mkc_path_t pathtype, char *buff, size_t sz, char *filename, mkc_error_t *mkcerr);
void mkc_path_set_dir (mkc_dir_t dir, const char *path);
void mkc_getcwd (char *buff, size_t sz);
void mkc_realpath (char *path, size_t sz);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_PATH_H */
