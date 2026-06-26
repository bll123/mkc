/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *    (from ballroomdj4)
 */
#ifndef MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dirent.h>

#if __has_include (<windows.h>)
# define WIN32_LEAN_AND_MEAN 1
# include <windows.h>
#endif

#include "mkc_fileop.h"
#include "mkc_osdir.h"
#include "mkc_string.h"

typedef struct dirhandle {
  DIR       *dh;
  char      *dirname;
#if _typ_HANDLE
  HANDLE    dhandle;
#endif
} dirhandle_t;

MKC_NODISCARD
dirhandle_t *
mkc_osdir_open (const char *dirname)
{
  dirhandle_t   *dirh;

  dirh = malloc (sizeof (dirhandle_t));
  dirh->dirname = NULL;
  dirh->dh = NULL;

#if _lib_FindFirstFileW
  {
    size_t        len = 0;
    char          *p;
    char          *end;

    dirh->dhandle = INVALID_HANDLE_VALUE;
    len = strlen (dirname) + 3;
    dirh->dirname = malloc (len);
    p = dirh->dirname;
    end = dirh->dirname + len;
    p = stpecpy (p, end, dirname);
    stringTrimChar (dirh->dirname, '/');
    p = stpecpy (p, end, "/*");
  }
#else
  dirh->dirname = strdup (dirname);
  dirh->dh = opendir (dirname);
#endif

  return dirh;
}

MKC_NODISCARD
char *
mkc_osdir_iterate (dirhandle_t *dirh)
{
  char      *fname = NULL;
#if _lib_FindFirstFileW
  WIN32_FIND_DATAW filedata;
  BOOL             rc;
#else
  struct dirent   *dirent = NULL;
#endif

  if (dirh == NULL) {
    return NULL;
  }

#if _lib_FindFirstFileW
  if (dirh->dhandle == INVALID_HANDLE_VALUE) {
    wchar_t         *wdirname;

    wdirname = mkc_towide (dirh->dirname);
    dirh->dhandle = FindFirstFileW (wdirname, &filedata);
    rc = 0;
    if (dirh->dhandle != INVALID_HANDLE_VALUE) {
      rc = 1;
    }
    free (wdirname);
  } else {
    rc = FindNextFileW (dirh->dhandle, &filedata);
  }

  fname = NULL;
  if (rc != 0) {
    fname = mkc_fromwide (filedata.cFileName);
  }
#else
  if (dirh->dh == NULL) {
    return NULL;
  }

  dirent = readdir (dirh->dh);
  fname = NULL;
  if (dirent != NULL) {
    fname = strdup (dirent->d_name);
  }
#endif

  return fname;
}

void
mkc_osdir_close (dirhandle_t *dirh)
{
  if (dirh == NULL) {
    return;
  }

#if _lib_FindFirstFileW
  if (dirh->dhandle != INVALID_HANDLE_VALUE) {
    FindClose (dirh->dhandle);
  }
  dirh->dhandle = INVALID_HANDLE_VALUE;
#else
  if (dirh->dh != NULL) {
    closedir (dirh->dh);
  }
  dirh->dh = NULL;
#endif
  datafree (dirh->dirname);
  dirh->dirname = NULL;
  free (dirh);
}
