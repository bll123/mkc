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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>
#include <errno.h>
#include <dirent.h>

#if __has_include (<windows.h>)
# define WIN32_LEAN_AND_MEAN 1
# include <windows.h>
#endif

#include "mkc_def.h"
#include "dirop.h"
#include "mkc_error.h"
#include "mkc_list.h"
#include "fileop.h"
#include "mkc_nodiscard.h"
#include "strutil.h"

typedef struct mkc_dirhandle_t {
  DIR       *dh;
  char      *dirname;
#if _type_HANDLE
  HANDLE    dhandle;
#endif
} mkc_dirhandle_t;

static int dirop_make_recursive (const char *dirname, mkc_error_t *mkcerr);
static int dirop_makedir (const char *dirname, mkc_error_t *mkcerr);

MKC_NODISCARD
mkc_dirhandle_t *
dirop_open (const char *dirname, mkc_error_t *mkcerr)
{
  mkc_dirhandle_t   *dirh;

  dirh = malloc (sizeof (mkc_dirhandle_t));
  if (dirh == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  dirh->dirname = NULL;
  dirh->dh = NULL;

#if _function_FindFirstFileW
  {
    size_t        len = 0;
    char          *p;
    char          *end;

    dirh->dhandle = INVALID_HANDLE_VALUE;
    len = strlen (dirname) + 3;
    dirh->dirname = malloc (len);
    if (dirh->dirname == NULL) {
      mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return NULL;
    }
    p = dirh->dirname;
    end = dirh->dirname + len;
    p = stpecpy (p, end, dirname);
    str_trim_char (dirh->dirname, '/');
    p = stpecpy (p, end, "/*");
  }
#else
  dirh->dirname = strdup (dirname);
  if (dirh->dirname == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  dirh->dh = opendir (dirname);
#endif

  return dirh;
}

MKC_NODISCARD
char *
dirop_iterate (mkc_dirhandle_t *dirh, mkc_error_t *mkcerr)
{
  char      *fname = NULL;

  if (dirh == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

#if _function_FindFirstFileW
  {
    WIN32_FIND_DATAW filedata;
    BOOL             rc;

    if (dirh->dhandle == INVALID_HANDLE_VALUE) {
      wchar_t         *wdirname;

      wdirname = str_towide (dirh->dirname);
      if (wdirname == NULL) {
        mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
        return NULL;
      }
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
      fname = str_fromwide (filedata.cFileName);
      if (fname == NULL) {
        mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
        return NULL;
      }
    }
  }
#else
  {
    struct dirent   *dirent = NULL;

    if (dirh->dh == NULL) {
      return NULL;
    }

    dirent = readdir (dirh->dh);
    fname = NULL;
    if (dirent != NULL) {
      fname = strdup (dirent->d_name);
      if (fname == NULL) {
        mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
        return NULL;
      }
    }
  }
#endif

  return fname;
}

void
dirop_close (mkc_dirhandle_t *dirh)
{
  if (dirh == NULL) {
    return;
  }

#if _function_FindFirstFileW
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

int
dirop_make (const char *dirname, mkc_error_t *mkcerr)
{
  int rc;
  rc = dirop_make_recursive (dirname, mkcerr);
  return rc;
}

/* deletes the directory */
/* if the directory only contains other dirs, the empty dirs will */
/* be recursively deleted */
int
dirop_delete (const char *dirname, int flags, mkc_error_t *mkcerr)
{
  mkc_dirhandle_t *dh;
  char            *fname;
  char            temp [MKC_PATH_MAX];

  if (fileop_is_directory (dirname)) {
    /* not an error */
    return MKC_OK;
  }

  if (! fileop_is_directory (dirname)) {
    mkc_error_set (mkcerr, MKC_ERR_DIR_NOT_A_DIR, 0, dirname);
    return MKC_ERR_FAILURE;
  }

  dh = dirop_open (dirname, mkcerr);
  if (dh == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_DIR_UNABLE_TO_OPEN, errno, dirname);
    return MKC_ERR_FAILURE;
  }

  while ((fname = dirop_iterate (dh, mkcerr)) != NULL) {
    if (strcmp (fname, ".") == 0 ||
        strcmp (fname, "..") == 0) {
      free (fname);
      continue;
    }

    snprintf (temp, sizeof (temp), "%s/%s", dirname, fname);
    if (fname != NULL) {
      if (fileop_is_directory (temp)) {
        if (! dirop_delete (temp, flags, mkcerr)) {
          free (fname);
          dirop_close (dh);
          return MKC_ERR_FAILURE;
        }
      } else {
        if ((flags & DIROP_ONLY_IF_EMPTY) == DIROP_ONLY_IF_EMPTY) {
          free (fname);
          dirop_close (dh);
          return MKC_ERR_FAILURE;
        }
        fileop_file_delete (temp);
      }
    }
    free (fname);
  }

  dirop_close (dh);

  /* in case the dir is actually a link... */
  fileop_file_delete (dirname);

#if _function_RemoveDirectoryW
  {
    wchar_t       * tdirname;

    tdirname = str_towide (dirname);
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

MKC_NODISCARD
mkc_list_t *
dirop_basic_list (const char *dirname, mkc_error_t *mkcerr)
{
  mkc_dirhandle_t *dh;
  char            *fname;
  mkc_list_t      *filelist;
  char            temp [MKC_PATH_MAX];

  if (! fileop_is_directory (dirname)) {
    mkc_error_set (mkcerr, MKC_ERR_DIR_NOT_A_DIR, 0, dirname);
    return NULL;
  }

  filelist = mkc_list_init (MKC_LIST_UNSORTED, mkc_list_ind_free, NULL, mkcerr);
  dh = dirop_open (dirname, mkcerr);
  while ((fname = dirop_iterate (dh, mkcerr)) != NULL) {
    mkc_listidx_t   loc;

    snprintf (temp, sizeof (temp), "%s/%s", dirname, fname);
    if (fileop_is_directory (temp)) {
      free (fname);
      continue;
    }

    mkc_list_set (filelist, &fname, sizeof (char *), &loc);
  }
  dirop_close (dh);

  return filelist;
}

MKC_NODISCARD
mkc_list_t *
dirop_list_recursive (const char *dirname, int flags, mkc_error_t *mkcerr)
{
  mkc_dirhandle_t *dh;
  char            *fname;
  mkc_list_t      *filelist;
  mkc_list_t      *dirqueue;
  char            temp [MKC_PATH_MAX];
  char            *p;
//  size_t          dirnamelen;
  mkc_listidx_t   loc;
  int32_t         processed = 0;

  if (! fileop_is_directory (dirname)) {
    mkc_error_set (mkcerr, MKC_ERR_DIR_NOT_A_DIR, 0, dirname);
    return NULL;
  }

//  dirnamelen = strlen (dirname);
  filelist = mkc_list_init (MKC_LIST_UNSORTED, mkc_list_ind_free, NULL, mkcerr);
  dirqueue = mkc_list_init (MKC_LIST_UNSORTED, mkc_list_ind_free, NULL, mkcerr);

  p = strdup (dirname);
  mkc_list_set (dirqueue, &p, sizeof (char *), &loc);
  while (mkc_list_size (dirqueue) - processed > 0) {
    char  *dir;

    dir = mkc_list_get_by_idx (dirqueue, processed);
    processed += 1;

    dh = dirop_open (dir, mkcerr);
    while ((fname = dirop_iterate (dh, mkcerr)) != NULL) {
      char    *tp;

      if (strcmp (fname, ".") == 0 ||
          strcmp (fname, "..") == 0) {
        free (fname);
        continue;
      }

      snprintf (temp, sizeof (temp), "%s/%s", dir, fname);
      if ((flags & DIRLIST_LINKS) == DIRLIST_LINKS &&
          fileop_is_link (temp)) {
        if ((flags & DIRLIST_FILES) == DIRLIST_FILES) {
          // p = temp + dirnamelen + 1;
          tp = strdup (temp);
          mkc_list_set (filelist, &tp, sizeof (char *), &loc);
        }
      } else if (fileop_is_directory (temp)) {
        tp = strdup (temp);
        mkc_list_set (dirqueue, &tp, sizeof (char *), &loc);
        if ((flags & DIRLIST_DIRS) == DIRLIST_DIRS) {
          // p = temp + dirnamelen + 1;
          tp = strdup (temp);
          mkc_list_set (filelist, &tp, sizeof (char *), &loc);
        }
      } else if (fileop_exists (temp)) {
        if ((flags & DIRLIST_FILES) == DIRLIST_FILES) {
          // p = temp + dirnamelen + 1;
          tp = strdup (temp);
          mkc_list_set (filelist, &tp, sizeof (char *), &loc);
        }
      }
      free (fname);
    }

    dirop_close (dh);
    free (dir);
  }
  mkc_list_free (dirqueue);

  return filelist;
}

/* internal routines */

static int
dirop_make_recursive (const char *dirname, mkc_error_t *mkcerr)
{
  char    tbuff [MKC_PATH_MAX];
  char    *p = NULL;
  int     rc = MKC_OK;

  stpecpy (tbuff, tbuff + MKC_PATH_MAX, dirname);
  str_trim_char (tbuff, '/');

  for (p = tbuff + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      rc = dirop_makedir (tbuff, mkcerr);
      if (rc != 0) {
        break;
      }
      *p = '/';
    }
  }
  if (rc == 0) {
    rc = dirop_makedir (tbuff, mkcerr);
  }
  return rc;
}

static int
dirop_makedir (const char *dirname, mkc_error_t *mkcerr)
{
  int   rc = MKC_ERR_FAILURE;

  if (fileop_is_directory (dirname)) {
    return MKC_OK;
  }

/* windows */
#if _arg_count_mkdir == 1
  wchar_t   *tdirname = NULL;

  tdirname = str_towide (dirname);
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
