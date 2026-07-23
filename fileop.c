/*
 * Copyright 2023-2026 Brad Lanam Pleasant Hill CA
 *    (from libmp4tag, ballroomdj4)
 */

#ifndef MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <wchar.h>
#include <errno.h>

#if __has_include (<windows.h>)
# define WIN32_LEAN_AND_MEAN 1
# include <windows.h>
#endif
#if __has_include (<io.h>)
# include <io.h>
#endif

#include "mkc_def.h"
#include "mkc_error.h"
#include "fileop.h"
#include "mkc_nodiscard.h"
#include "strutil.h"

MKC_NODISCARD
FILE *
fileop_open (const char *fname, const char *mode)
{
  FILE          *fh = NULL;

#if defined (MKC_SYS_WIN)
  {
    wchar_t       *tfname = NULL;
    wchar_t       *tmode = NULL;

    tfname = str_towide (fname);
    tmode = str_towide (mode);
    if (tfname != NULL && tmode != NULL) {
      fh = _wfopen (tfname, tmode);
      free (tfname);
      free (tmode);
    }
  }
#else
  {
    fh = fopen (fname, mode);
  }
#endif
  return fh;
}

bool
fileop_exists (const char *fname)
{
  int     rc = -1;
  bool    brc = false;

#if _function__wstat64 || (MKC_BOOTSTRAP && MKC_SYS_WIN)
  {
    struct __stat64  statbuf;
    wchar_t       *tfname = NULL;

    tfname = str_towide (fname);
    if (tfname != NULL) {
      rc = _wstat64 (tfname, &statbuf);
      free (tfname);
    }
  }
#else
  {
    struct stat statbuf;

    rc = stat (fname, &statbuf);
  }
#endif
  if (rc == 0) {
    brc = true;
  }

  return brc;
}

time_t
fileop_modtime (const char *fname)
{
  time_t      tm = 0;

#if _function__wstat64 || (MKC_BOOTSTRAP && MKC_SYS_WIN)
  {
    struct __stat64  statbuf;
    wchar_t       *tfname = NULL;
    int           rc;

    tfname = str_towide (fname);
    if (tfname != NULL) {
      rc = _wstat64 (tfname, &statbuf);
      if (rc == 0) {
        tm = statbuf.st_mtime;
      }
      free (tfname);
    }
  }
#else
  {
    int rc;
    struct stat statbuf;

    rc = stat (fname, &statbuf);
    if (rc == 0) {
      tm = statbuf.st_mtime;
    }
  }
#endif
  return tm;
}

ssize_t
fileop_size (const char *fname)
{
  ssize_t       sz = -1;

#if _function__wstat64 || (MKC_BOOTSTRAP && MKC_SYS_WIN)
  {
    struct __stat64  statbuf;
    wchar_t       *tfname = NULL;
    int           rc;

    tfname = str_towide (fname);
    if (tfname != NULL) {
      rc = _wstat64 (tfname, &statbuf);
      if (rc == 0) {
        sz = statbuf.st_size;
      }
      free (tfname);
    }
  }
#else
  {
    int rc;
    struct stat statbuf;

    rc = stat (fname, &statbuf);
    if (rc == 0) {
      sz = statbuf.st_size;
    }
  }
#endif
  return sz;
}

bool
fileop_is_link (const char *fname)
{
  int   rc = -1;
  bool  brc = false;

#if _function_symlink
  {
    struct stat   statbuf;

    memset (&statbuf, '\0', sizeof (struct stat));
    rc = stat (fname, &statbuf);
    if (rc == 0 && (statbuf.st_mode & S_IFMT) != S_IFLNK) {
      rc = -1;
    }
  }
#endif
  /* had some trouble with the optimizer, so spell out the values */
  if (rc == 0) {
    brc = true;
  }

  return brc;
}

MKC_NODISCARD
char *
fileop_read_file (const char *fn, size_t *sz, mkc_error_t *mkcerr)
{
  char    *fdata = NULL;
  int     rc = -1;
  FILE    *fh;
  ssize_t fsz;

  *sz = 0;
  fsz = fileop_size (fn);
  if (fsz < 0) {
    mkc_error_set (mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, fn);
    return NULL;
  }

  fdata = malloc (fsz + 1);
  if (fdata == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  fh = fileop_open (fn, "rb");
  if (fh == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, fn);
  } else {
    rc = fread (fdata, fsz, 1, fh);
    if (rc != 1) {
      mkc_error_set (mkcerr, MKC_ERR_FILE_READ_ERROR, errno, fn);
    }
  }
  fclose (fh);

  if (rc != 1 && fdata != NULL) {
    free (fdata);
    fdata = NULL;
    fsz = 0;
  }

  *sz = fsz;
  fdata [fsz] = '\0';
  return fdata;
}

int
fileop_file_delete (const char *fname)
{
  int     rc = -1;
#if _function__wunlink || (MKC_BOOTSTRAP && MKC_SYS_WIN)
  wchar_t *tname;
#endif

  if (fname == NULL) {
    return MKC_ERR_NULL_ARGUMENT;
  }

#if _function__wunlink || (MKC_BOOTSTRAP && MKC_SYS_WIN)
  tname = str_towide (fname);
  if (tname != NULL) {
    rc = _wunlink (tname);
    free (tname);
  }
#else
  rc = unlink (fname);
#endif
  return rc;
}

int
fileop_file_move (const char *fname, const char *nfn)
{
  int       rc = MKC_ERR_NULL_ARGUMENT;

  if (fname == NULL || nfn == NULL) {
    return rc;
  }

#if _function__wrename || (MKC_BOOTSTRAP && MKC_SYS_WIN)
  /*
   * Windows won't rename to an existing file, but does
   * not return an error.
   */
  fileop_file_delete (nfn);
  {
    wchar_t   *wfname;
    wchar_t   *wnfn;

    wfname = str_towide (fname);
    wnfn = str_towide (nfn);
    if (wfname != NULL && wnfn != NULL) {
      rc = _wrename (wfname, wnfn);
      free (wfname);
      free (wnfn);
    }
  }
#else
  rc = rename (fname, nfn);
#endif
  return rc;
}

int64_t
fileop_tell (FILE *fh)
{
#if _function_ftello
  return ftello (fh);
#else
  return ftell (fh);
#endif
}

int
fileop_seek (FILE *fh, int64_t offset, int whence)
{
#if _function_fseeko
  return fseeko (fh, offset, whence);
#else
  return fseek (fh, (long) offset, whence);
#endif
}

int
fileop_file_copy (const char *fname, const char *nfn, mkc_error_t *mkcerr)
{
  int     rc = -1;

#if _function_CopyFileW || (MKC_BOOTSTRAP && defined MKC_SYS_WIN)
  char      tfname [MKC_PATH_MAX];
  char      tnfn [MKC_PATH_MAX];

  stpecpy (tfname, tfname + sizeof (tfname), fname);
  fileop_display_path (tfname, sizeof (tfname));
  stpecpy (tnfn, tnfn + sizeof (tnfn), nfn);
  fileop_display_path (tnfn, sizeof (tnfn));
  {
    wchar_t   *wtfname;
    wchar_t   *wtnfn;

    wtfname = str_towide (tfname);
    wtnfn = str_towide (tnfn);

    rc = CopyFileW (wtfname, wtnfn, 0);
    rc = rc != 0 ? 0 : -1;
    free (wtfname);
    free (wtnfn);
  }
#else
  char    *data;
  FILE    *fh;
  size_t  len;
  size_t  trc = 0;

  data = fileop_read_file (fname, &len, mkcerr);
  if (data != NULL && mkc_error_chk_ok (mkcerr)) {
    fh = fileop_open (nfn, "w");
    if (fh != NULL) {
      trc = fwrite (data, len, 1, fh);
      fclose (fh);
    }
    free (data);
  }
  rc = trc == 1 ? 0 : -1;
#endif

  return rc;
}

/* link if possible, otherwise copy */
/* requires a proper path for the link */
/* windows has had symlinks for ages, but they require either */
/* admin permission, or have the machine in developer mode */
/* shell links would be fine probably, but creating them is a hassle */
int
fileop_link_copy (const char *fname, const char *nfn, mkc_error_t *mkcerr)
{
  int       rc = -1;

#if _function_symlink
  rc = fileop_link_create (fname, nfn);
#else
  rc = fileop_file_copy (fname, nfn, mkcerr);
#endif
  return rc;
}

void
fileop_display_path (char *path, size_t sz)
{
  /* a no-op on unix systems */
#if MKC_SYS_WIN
  for (size_t i = 0; i < sz; ++i) {
    if (path [i] == '\0') {
      break;
    }
    if (path [i] == '/') {
      path [i] = '\\';
    }
  }
#endif
  return;
}

void
fileop_normalize_path (char *path, size_t sz)
{
  for (size_t i = 0; i < sz; ++i) {
    if (path [i] == '\0') {
      break;
    }
    if (path [i] == '\\') {
      path [i] = '/';
    }
  }
  return;
}

#if _function_symlink

int
fileop_link_create (const char *target, const char *linkpath)
{
  int rc;

  rc = symlink (target, linkpath);
  return rc;
}

#endif /* lib_symlink */

/* directory operations */

bool
fileop_is_directory (const char *fname)
{
  int   rc = -1;
  bool  brc = false;

#if _function__wstat64
  {
    struct __stat64  statbuf;
    wchar_t       *tfname = NULL;

    tfname = str_towide (fname);
    rc = _wstat64 (tfname, &statbuf);
    if (rc == 0 && (statbuf.st_mode & S_IFMT) != S_IFDIR) {
      rc = -1;
    }
    free (tfname);
  }
#else
  {
    struct stat   statbuf;

    memset (&statbuf, '\0', sizeof (struct stat));
    rc = stat (fname, &statbuf);
    if (rc == 0 && (statbuf.st_mode & S_IFMT) != S_IFDIR) {
      rc = -1;
    }
  }
#endif
  /* had some trouble with the optimizer, so spell out the values */
  if (rc == 0) {
    brc = true;
  }

  return brc;
}

