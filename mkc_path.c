/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#if ! MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_path.h"
#include "mkc_string.h"

#if ! defined (MKC_DIR_SHARE)
  // #define MKC_DIR_SHARE "mkc"
  /* this works for development, not for a mkc/ dir included with a project */
  #define MKC_DIR_SHARE "."
#endif

/* this works for development, not for a mkc/ dir included with a project */
/* mkc/mkc_files */
static char mkc_dir_mkc_files [MKC_PATH_MAX] = "mkc_files";

void
mkc_path_build (mkc_path_t pathtype, char *buff, size_t sz,
    char *filename, mkc_error_t *mkcerr)
{
  char    *p = NULL;

  if (pathtype == MKC_PATH_SHARE) {
    /* the system share directory */
    p = stpecpy (buff, buff + sz, MKC_DIR_SHARE);
  } else if (pathtype == MKC_PATH_MKC_FILES) {
    /* the mkc_files/ directory; this can be changed at run-time */
    p = stpecpy (buff, buff + sz, mkc_dir_mkc_files);
  } else if (pathtype == MKC_PATH_MKC_TEMPLATES) {
    /* this directory has the language templates for the checks */
    p = stpecpy (buff, buff + sz, MKC_DIR_SHARE);
    p = stpecpy (p, buff + sz, "/templates");
  } else if (pathtype == MKC_PATH_MKC_TMP) {
    /* the tmp/ directory in mkc_files/ */
    p = stpecpy (buff, buff + sz, mkc_dir_mkc_files);
    p = stpecpy (p, buff + sz, "/tmp");
  } else if (pathtype == MKC_PATH_MKC_INCLUDE) {
    /* this directory has the mkc_def.h and mkc_compiler.h files in it, */
    /* used by the various checks */
    p = stpecpy (buff, buff + sz, MKC_DIR_SHARE);
    p = stpecpy (p, buff + sz, "/include");
  } else {
    mkc_error_set (mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
    return;
  }

  if (filename != NULL && *filename) {
    p = stpecpy (p, buff + sz, "/");
    p = stpecpy (p, buff + sz, filename);
  }
}

void
mkc_path_set_dir_mkc_files (const char *path)
{
  stpecpy (mkc_dir_mkc_files, mkc_dir_mkc_files + sizeof (mkc_dir_mkc_files), path);
}
