/*
 * Copyright 2023-2025 Brad Lanam Pleasant Hill CA
 *    (from libmp4tag)
 */

#if ! MKC_BOOTSTRAP
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
#include "mkc_fileop.h"
#include "mkc_nodiscard.h"
#include "mkc_string.h"

MKC_NODISCARD
FILE *
mkc_fopen (const char *fname, const char *mode)
{
  FILE          *fh = NULL;

#if defined (_WIN32)
  {
    wchar_t       *tfname = NULL;
    wchar_t       *tmode = NULL;

    tfname = mkc_towide (fname);
    tmode = mkc_towide (mode);
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

MKC_NODISCARD
time_t
mkc_file_modtime (const char *fname)
{
  time_t      tm = 0;

#if _lib__wstat64 || (MKC_BOOTSTRAP && _WIN32)
  {
    struct __stat64  statbuf;
    wchar_t       *tfname = NULL;
    int           rc;

    tfname = mkc_towide (fname);
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

MKC_NODISCARD
ssize_t
mkc_file_size (const char *fname)
{
  ssize_t       sz = -1;

#if _lib__wstat64 || (MKC_BOOTSTRAP && _WIN32)
  {
    struct __stat64  statbuf;
    wchar_t       *tfname = NULL;
    int           rc;

    tfname = mkc_towide (fname);
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

MKC_NODISCARD
char *
mkc_read_file (const char *fn, size_t *sz, mkc_error_t *mkcerr)
{
  char    *fdata = NULL;
  int     rc = -1;
  FILE    *fh;
  ssize_t fsz;

  *sz = 0;
  fsz = mkc_file_size (fn);
  if (fsz < 0) {
    mkc_error_set (mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, fn);
    return NULL;
  }

  fdata = malloc (fsz + 1);
  if (fdata == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  fh = mkc_fopen (fn, "rb");
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
mkc_file_delete (const char *fname)
{
  int     rc = -1;
#if _lib__wunlink || (MKC_BOOTSTRAP && _WIN32)
  wchar_t *tname;
#endif

  if (fname == NULL) {
    return MKC_ERR_NULL_ARGUMENT;
  }

#if _lib__wunlink || (MKC_BOOTSTRAP && _WIN32)
  tname = mkc_towide (fname);
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
mkc_file_move (const char *fname, const char *nfn)
{
  int       rc = MKC_ERR_NULL_ARGUMENT;

  if (fname == NULL || nfn == NULL) {
    return rc;
  }

#if _lib__wrename || (MKC_BOOTSTRAP && _WIN32)
  /*
   * Windows won't rename to an existing file, but does
   * not return an error.
   */
  mkc_file_delete (nfn);
  {
    wchar_t   *wfname;
    wchar_t   *wnfn;

    wfname = mkc_towide (fname);
    wnfn = mkc_towide (nfn);
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
mkc_ftell (FILE *fh)
{
#if _lib_ftello
  return ftello (fh);
#else
  return ftell (fh);
#endif
}

int
mkc_fseek (FILE *fh, int64_t offset, int whence)
{
#if _lib_fseeko
  return fseeko (fh, offset, whence);
#else
  return fseek (fh, (long) offset, whence);
#endif
}

int
mkc_file_copy (const char *fname, const char *nfn, mkc_error_t *mkcerr)
{
  int     rc = -1;

#if _lib_CopyFileW || (MKC_BOOTSTRAP && defined _WIN32)
  char      tfname [MKC_PATH_MAX];
  char      tnfn [MKC_PATH_MAX];

  stpecpy (tfname, tfname + sizeof (tfname), fname);
  mkc_disppath (tfname, sizeof (tfname));
  stpecpy (tnfn, tnfn + sizeof (tnfn), nfn);
  mkc_disppath (tnfn, sizeof (tnfn));
  {
    wchar_t   *wtfname;
    wchar_t   *wtnfn;

    wtfname = mkc_towide (tfname);
    wtnfn = mkc_towide (tnfn);

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

  data = mkc_read_file (fname, &len, mkcerr);
  if (data != NULL && mkc_error_chk_ok (mkcerr)) {
    fh = mkc_fopen (nfn, "w");
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
mkc_link_copy (const char *fname, const char *nfn, mkc_error_t *mkcerr)
{
  int       rc = -1;

#if _lib_symlink || (MKC_BOOTSTRAP && ! _WIN32)
  rc = mkc_link_create (fname, nfn);
#else
  rc = mkc_file_copy (fname, nfn, mkcerr);
#endif
  return rc;
}

void
mkc_disppath (char *path, size_t sz)
{
  for (size_t i = 0; i < sz; ++i) {
    if (path [i] == '\0') {
      break;
    }
    if (path [i] == '/') {
      path [i] = '\\';
    }
  }
  return;
}

void
mkc_normalizepath (char *path, size_t sz)
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

#if _lib_symlink || (MKC_BOOTSTRAP && ! _WIN32)

int
mkc_link_create (const char *target, const char *linkpath)
{
  int rc;

  rc = symlink (target, linkpath);
  return rc;
}

#endif /* lib_symlink */

