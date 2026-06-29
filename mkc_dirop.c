/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *    (from ballroomdj4)
 */

#if ! defined (MKC_BOOTSTRAP)
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>
#include <errno.h>

#if __has_include (<windows.h>)
# define WIN32_LEAN_AND_MEAN 1
# include <windows.h>
#endif

#include "mkc_def.h"
#include "mkc_dirop.h"
#include "mkc_error.h"
#include "mkc_fileop.h"
#include "mkc_osdir.h"
#include "mkc_string.h"

static int mkc_dirop_make_recursive (const char *dirname, mkc_error_t *mkcerr);
static int mkc_dirop_makedir (const char *dirname, mkc_error_t *mkcerr);

int
mkc_dirop_make (const char *dirname, mkc_error_t *mkcerr)
{
  int rc;
  rc = mkc_dirop_make_recursive (dirname, mkcerr);
  return rc;
}

/* deletes the directory */
/* if the directory only contains other dirs, the empty dirs will */
/* be recursively deleted */
int
mkc_dirop_delete (const char *dirname, int flags, mkc_error_t *mkcerr)
{
  dirhandle_t   *dh;
  char          *fname;
  char          temp [MKC_PATH_MAX];

  if (mkc_is_directory (dirname)) {
    /* not an error */
    return MKC_OK;
  }

  if (! mkc_is_directory (dirname)) {
    mkc_error_set (mkcerr, MKC_ERR_DIR_NOT_A_DIR, 0, dirname);
    return MKC_ERR_FAILURE;
  }

  dh = mkc_osdir_open (dirname);
  if (dh == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_DIR_UNABLE_TO_OPEN, errno, dirname);
    return MKC_ERR_FAILURE;
  }

  while ((fname = mkc_osdir_iterate (dh)) != NULL) {
    if (strcmp (fname, ".") == 0 ||
        strcmp (fname, "..") == 0) {
      free (fname);
      continue;
    }

    snprintf (temp, sizeof (temp), "%s/%s", dirname, fname);
    if (fname != NULL) {
      if (mkc_is_directory (temp)) {
        if (! mkc_dirop_delete (temp, flags, mkcerr)) {
          free (fname);
          mkc_osdir_close (dh);
          return MKC_ERR_FAILURE;
        }
      } else {
        if ((flags & DIROP_ONLY_IF_EMPTY) == DIROP_ONLY_IF_EMPTY) {
          free (fname);
          mkc_osdir_close (dh);
          return MKC_ERR_FAILURE;
        }
        mkc_file_delete (temp);
      }
    }
    free (fname);
  }

  mkc_osdir_close (dh);

  /* in case the dir is actually a link... */
  mkc_file_delete (dirname);

#if _lib_RemoveDirectoryW
  {
    wchar_t       * tdirname;

    tdirname = mkc_towide (dirname);
    if (tdirname == NULL) {
      mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return MKC_ERR_FAILURE;
    }
    RemoveDirectoryW (tdirname);
    free (tdirname);
  }
#else
  rmdir (dirname);
#endif

  return MKC_OK;
}

/* internal routines */

static int
mkc_dirop_make_recursive (const char *dirname, mkc_error_t *mkcerr)
{
  char    tbuff [MKC_PATH_MAX];
  char    *p = NULL;
  int     rc = MKC_OK;

  if (mkc_is_directory (dirname)) {
    /* not an error */
    return MKC_OK;
  }

  stpecpy (tbuff, tbuff + MKC_PATH_MAX, dirname);
  mkc_trim_char (tbuff, '/');

  for (p = tbuff + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      rc = mkc_dirop_makedir (tbuff, mkcerr);
      if (rc != 0) {
        break;
      }
      *p = '/';
    }
  }
  if (rc == 0) {
    rc = mkc_dirop_makedir (tbuff, mkcerr);
  }
  return rc;
}

static int
mkc_dirop_makedir (const char *dirname, mkc_error_t *mkcerr)
{
  int   rc = MKC_ERR_FAILURE;

/* windows */
#if _arg_count_mkdir == 1
  wchar_t   *tdirname = NULL;

  tdirname = mkc_towide (dirname);
  if (tdirname == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }
  rc = _wmkdir (tdirname);
  free (tdirname);
#endif
#if _arg_count_mkdir == 2 && _define_S_IRWXU
  rc = mkdir (dirname, S_IRWXU);
#endif
  if (rc != 0) {
    mkc_error_set (mkcerr, MKC_ERR_DIR_NOT_CREATED, errno, dirname);
  }
  return rc;
}
