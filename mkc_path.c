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
  [MKC_DIR_PREFIX] = "",
  [MKC_DIR_SHARE] = "",
};

const char * const pathdesc [MKC_PATH_BUILD_MAX] = {
  [MKC_PATH_CONFIG] = "config",
  [MKC_PATH_EXEC_PATH] = "exec",
  [MKC_PATH_HOME] = "home",
  [MKC_PATH_MKC_TEMPLATES] = "mkc_templates",
  [MKC_PATH_MKC_FILES] = "mkc_files",
  [MKC_PATH_MKC_TMP] = "mkc_tmp",
  [MKC_PATH_MKC_INCLUDE] = "mkc_include",
  [MKC_PATH_MKC_UNITS] = "mkc_units",
  [MKC_PATH_MKC_USER_UNITS] = "mkc_user_units",
  [MKC_PATH_SHARE] = "share",
  [MKC_PATH_PREFIX] = "prefix",
};

static bool gmkcpathinit = false;

static void mkc_path_init (mkc_error_t *mkcerr);
static char * mkc_path_config (char *buff, size_t sz);

void
mkc_path_build (mkc_path_t pathtype, char *buff, size_t sz,
    char *filename, mkc_error_t *mkcerr)
{
  char        *p = NULL;

  mkc_path_init (mkcerr);

  switch (pathtype) {
    case MKC_PATH_CONFIG: {
      p = mkc_path_config (buff, sz);
      break;
    }
    case MKC_PATH_EXEC_PATH: {
      /* the path to the mkc executable */
      p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_EXEC]);
      break;
    }
    case MKC_PATH_HOME: {
      p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_HOME]);
      break;
    }
    case MKC_PATH_MKC_FILES: {
      /* the mkc_files/ directory; this can be changed at run-time */
      p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_MKC_FILES]);
      break;
    }
    case MKC_PATH_MKC_INCLUDE: {
      /* this directory has the mkc_def.h and mkc_compiler.h files in it, */
      /* used by the various checks */
      p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_SHARE]);
      p = stpecpy (p, buff + sz, "/include");
      break;
    }
    case MKC_PATH_MKC_TEMPLATES: {
      /* this directory has the language templates for the checks */
      p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_SHARE]);
      p = stpecpy (p, buff + sz, "/templates");
      break;
    }
    case MKC_PATH_MKC_UNITS: {
      /* this directory contains the .mkc unit files */
      p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_SHARE]);
      p = stpecpy (p, buff + sz, "/units");
      break;
    }
    case MKC_PATH_MKC_USER_UNITS: {
      /* this directory contains the user's .mkc unit files */
      p = mkc_path_config (buff, sz);
      p = stpecpy (p, buff + sz, "/units");
      break;
    }
    case MKC_PATH_MKC_TMP: {
      /* the tmp/ directory in mkc_files/ */
      p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_MKC_FILES]);
      p = stpecpy (p, buff + sz, "/tmp");
      break;
    }
    case MKC_PATH_PREFIX: {
      p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_PREFIX]);
      break;
    }
    case MKC_PATH_SHARE: {
      /* the system share directory */
      p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_SHARE]);
      break;
    }
    case MKC_PATH_BUILD_MAX: {
      mkc_error_set (mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
      return;
    }
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

static void
mkc_path_init (mkc_error_t *mkcerr)
{
  char        tbuff [MKC_PATH_MAX];
  char        *p = NULL;
  mkc_dir_t   dir;
  bool        islocal = false;

  if (gmkcpathinit) {
    return;
  }

  /* set up the path to the mkc_files/ directory */
  /* if in bootstrap, use local relative paths */
  /* if the templates/ directory is there, use local relative paths */
  dir = MKC_DIR_MKC_FILES;

  /* this is a special case for development purposes */
  p = stpecpy (tbuff, tbuff + sizeof (tbuff), mkc_dirs [MKC_DIR_EXEC]);
  p = stpecpy (p, tbuff + sizeof (tbuff), "/templates");
  if (mkc_is_directory (tbuff)) {
    islocal = true;
  }

  if (islocal) {
    /* the local flag overrides any share-directory that is set */
    stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, "mkc_files");
  }
  if (*mkc_dirs [dir] == '\0' && ! islocal) {
    stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, "mkc_files");
  }

  /* set up the path to the ../share/mkc/ directory */

  dir = MKC_DIR_SHARE;
#if defined (MKC_DIR_SHARE)
  p = stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, MKC_DIR_SHARE);
  p = stpecpy (p, mkc_dirs [dir] + MKC_PATH_MAX, "/mkc");
#endif
  if (islocal) {
    /* the local flag overrides any share-directory that is set */
    stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, mkc_dirs [MKC_DIR_EXEC]);
  }
  if (*mkc_dirs [dir] == '\0' && ! islocal) {
    p = stpecpy (mkc_dirs [dir], mkc_dirs [dir] + MKC_PATH_MAX, mkc_dirs [MKC_DIR_PREFIX]);
    p = stpecpy (p, mkc_dirs [dir] + MKC_PATH_MAX, "/share/mkc");
  }

  gmkcpathinit = true;
}

static char *
mkc_path_config (char *buff, size_t sz)
{
  char    *p;

  p = stpecpy (buff, buff + sz, mkc_dirs [MKC_DIR_HOME]);
#if MKC_SYS_WIN
  p = stpecpy (p, buff + sz, "/AppData/Roaming/mkc");
#else
  p = stpecpy (p, buff + sz, "/.config/mkc");
#endif

  return p;
}
