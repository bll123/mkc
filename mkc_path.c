/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#ifndef MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_fileop.h"
#include "mkc_path.h"
#include "mkc_string.h"

/* this works for development, not for a mkc/ dir included with a project */
/* mkc/mkc_files */
static char mkc_dirs [MKC_DIR_MAX][MKC_PATH_MAX] = {
  [MKC_DIR_HOME] = "",
  [MKC_DIR_MKC_FILES] = "",
  [MKC_DIR_SHARE] = "",
};

void
mkc_path_build (mkc_path_t pathtype, char *buff, size_t sz,
    char *filename, mkc_error_t *mkcerr)
{
  char        *p = NULL;
  mkc_dir_t   dir;

  dir = MKC_DIR_MKC_FILES;
  if (*mkc_dirs [dir] == '\0') {
    /* development */
    stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, "mkc_files");
  }
  dir = MKC_DIR_SHARE;
  if (*mkc_dirs [dir] == '\0') {
    /* development */
    stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, ".");
  }

  if (pathtype == MKC_PATH_CONFIG) {
#if _WIN32
    snprintf (buff, sz, "%s/AppData/Roaming/mkc", mkc_dirs [MKC_DIR_HOME]);
#else
    snprintf (buff, sz, "%s/.config/mkc", mkc_dirs [MKC_DIR_HOME]);
#endif
    p = buff + strlen (buff);
  } else if (pathtype == MKC_PATH_SHARE) {
    /* the system share directory */
    p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_SHARE]);
  } else if (pathtype == MKC_PATH_MKC_FILES) {
    /* the mkc_files/ directory; this can be changed at run-time */
    p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_MKC_FILES]);
  } else if (pathtype == MKC_PATH_MKC_TEMPLATES) {
    /* this directory has the language templates for the checks */
    p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_SHARE]);
    p = stpecpy (p, buff + sz, "/templates");
  } else if (pathtype == MKC_PATH_MKC_TMP) {
    /* the tmp/ directory in mkc_files/ */
    p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_MKC_FILES]);
    p = stpecpy (p, buff + sz, "/tmp");
  } else if (pathtype == MKC_PATH_MKC_INCLUDE) {
    /* this directory has the mkc_def.h and mkc_compiler.h files in it, */
    /* used by the various checks */
    p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_SHARE]);
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
mkc_path_set_dir (mkc_dir_t dir, const char *path)
{
  stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, path);
  mkc_normalize_path (mkc_dirs [dir], MKC_PATH_MAX);
}
