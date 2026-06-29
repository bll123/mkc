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

static char mkc_dirs [MKC_DIR_MAX][MKC_PATH_MAX] = {
  [MKC_DIR_EXEC] = "",
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
  int         isdev = true;

  /* set up the path to the mkc_files/ directory */
  /* if in bootstrap, use local relative paths */
  /* if the development flag is set, use local relative paths */
  dir = MKC_DIR_MKC_FILES;
#if defined (DEVELOPMENT)
  isdev = strcmp (DEVELOPMENT, "dev") == 0;
#endif
  if (isdev) {
    /* development */
    stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, "mkc_files");
  }
  if (*mkc_dirs [dir] == '\0' && ! isdev) {
    p = stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX,
        mkc_dirs [MKC_DIR_EXEC]);
    p = stpecpy (p, mkc_dirs [dir] + MKC_PATH_MAX, "/");
    p = stpecpy (p, mkc_dirs [dir] + MKC_PATH_MAX, "mkc_files");
  }

  /* set up the path to the ../share/mkc/ directory */

  dir = MKC_DIR_SHARE;
#if defined (MKC_DIR_SHARE)
  p = stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, MKC_DIR_SHARE);
  p = stpecpy (p, mkc_dirs [dir] + MKC_PATH_MAX, "/mkc");
#endif
  if (isdev) {
    /* the development flag overrides any share-directory that is set */
    stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, ".");
  }
  if (*mkc_dirs [dir] == '\0' && ! isdev) {
    stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, mkc_dirs [MKC_DIR_EXEC]);
  }

  p = NULL;

  if (pathtype == MKC_PATH_CONFIG) {
    p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_HOME]);
#if _WIN32
    p = stpecpy (p, buff + sz, "/AppData/Roaming/mkc");
#else
    p = stpecpy (p, buff + sz, "/.config/mkc");
#endif
  } else if (pathtype == MKC_PATH_EXEC_PATH) {
    /* the path to the mkc executable */
    p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_EXEC]);
  } else if (pathtype == MKC_PATH_HOME) {
    p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_HOME]);
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

  if (p != NULL && filename != NULL && *filename) {
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
