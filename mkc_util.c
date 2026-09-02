/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "dirop.h"
#include "mkc_compiler.h"
#include "mkc_util.h"
#include "pathutil.h"

bool
mkc_flag_is_libloc (mkc_compiler_id_t compid, const char *str)
{
  size_t    len;

  len = compiler_get_flag_len (compid, MKC_COMP_FLAG_LINKPREFIX);
  if (strncmp (str,
      compiler_get_flag (compid, MKC_COMP_FLAG_LINKPREFIX), len) == 0) {
    str += len;
  }

  if (strncmp (str,
      compiler_get_flag (compid, MKC_COMP_FLAG_LIBPATH),
      compiler_get_flag_len (compid, MKC_COMP_FLAG_LIBPATH)) == 0) {
    return true;
  }

  return false;
}

/* only cleans the obj/<name> and stage/<name> directories */
void
mkc_clean_mkcfiles (const char *project, char *tbuff, size_t tsz,
    mkc_error_t *mkcerr)
{
  /* clean out the obj/ and stage/ directory trees */
  path_build (MKC_PATH_MKCF_OBJECTS, tbuff, tsz, project, mkcerr);
  dirop_delete (tbuff, DIROP_ALL, mkcerr);
  path_build (MKC_PATH_MKCF_STAGE, tbuff, tsz, project, mkcerr);
  dirop_delete (tbuff, DIROP_ALL, mkcerr);
}

int
mkc_create_mkcfiles_tmp (char *tbuff, size_t tsz, mkc_error_t *mkcerr)
{
  int     rc;

  /* create the mkc_files temporary directory tree */
  path_build (MKC_PATH_MKCF_TMP, tbuff, tsz, NULL, mkcerr);
  rc = dirop_make (tbuff, mkcerr);

  return rc;
}

int
mkc_create_mkcfiles (char *tbuff, size_t tsz, mkc_error_t *mkcerr)
{
  int     rc;

  path_build (MKC_PATH_MKCF_OBJECTS, tbuff, tsz, NULL, mkcerr);
  rc = dirop_make (tbuff, mkcerr);
  if (rc != 0) {
    return rc;
  }
  path_build (MKC_PATH_MKCF_STAGE, tbuff, tsz, NULL, mkcerr);
  rc = dirop_make (tbuff, mkcerr);

  return rc;
}
