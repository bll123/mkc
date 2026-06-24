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
  MKC_PATH_MKC_FILES,       /* mkc_files */
  MKC_PATH_MKC_TMP,         /* mkc_files/tmp */
  MKC_PATH_MKC_INCLUDE,     /* .../share/mkc/include */
  MKC_PATH_MKC_TEMPLATES,   /* .../share/mkc/templates */
  MKC_PATH_SHARE,           /* .../share */
} mkc_path_t;

void mkc_path_build (mkc_path_t pathtype, char *buff, size_t sz, char *filename, mkc_error_t *mkcerr);
void mkc_path_set_dir_mkc_files (const char *path);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_PATH_H */
