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

#include "mkc_def.h"
#include "mkc_dirlist.h"
#include "mkc_error.h"
#include "mkc_fileop.h"
#include "mkc_nodiscard.h"
#include "mkc_string.h"

MKC_NODISCARD
mkc_list_t *
mkc_dir_basic_list (const char *dirname, mkc_error_t *mkcerr)
{
  mkc_dirhandle_t *dh;
  char            *fname;
  mkc_list_t      *filelist;
  char            temp [MKC_PATH_MAX];

  if (! mkc_is_directory (dirname)) {
    mkc_error_set (mkcerr, MKC_ERR_DIR_NOT_A_DIR, 0, dirname);
    return NULL;
  }

  filelist = mkc_list_init (MKC_LIST_UNSORTED, mkc_list_ind_free, NULL, mkcerr);
  dh = mkc_dir_open (dirname, mkcerr);
  while ((fname = mkc_dir_iterate (dh, mkcerr)) != NULL) {
    mkc_listidx_t   loc;

    snprintf (temp, sizeof (temp), "%s/%s", dirname, fname);
    if (mkc_is_directory (temp)) {
      free (fname);
      continue;
    }

    mkc_list_set (filelist, &fname, sizeof (char *), &loc);
  }
  mkc_dir_close (dh);

  return filelist;
}

MKC_NODISCARD
mkc_list_t *
mkc_dir_list_recursive (const char *dirname, int flags, mkc_error_t *mkcerr)
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

  if (! mkc_is_directory (dirname)) {
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

    dh = mkc_dir_open (dir, mkcerr);
    while ((fname = mkc_dir_iterate (dh, mkcerr)) != NULL) {
      char    *tp;

      if (strcmp (fname, ".") == 0 ||
          strcmp (fname, "..") == 0) {
        free (fname);
        continue;
      }

      snprintf (temp, sizeof (temp), "%s/%s", dir, fname);
      if ((flags & DIRLIST_LINKS) == DIRLIST_LINKS &&
          mkc_is_link (temp)) {
        if ((flags & DIRLIST_FILES) == DIRLIST_FILES) {
          // p = temp + dirnamelen + 1;
          tp = strdup (temp);
          mkc_list_set (filelist, &tp, sizeof (char *), &loc);
        }
      } else if (mkc_is_directory (temp)) {
        tp = strdup (temp);
        mkc_list_set (dirqueue, &tp, sizeof (char *), &loc);
        if ((flags & DIRLIST_DIRS) == DIRLIST_DIRS) {
          // p = temp + dirnamelen + 1;
          tp = strdup (temp);
          mkc_list_set (filelist, &tp, sizeof (char *), &loc);
        }
      } else if (mkc_file_exists (temp)) {
        if ((flags & DIRLIST_FILES) == DIRLIST_FILES) {
          // p = temp + dirnamelen + 1;
          tp = strdup (temp);
          mkc_list_set (filelist, &tp, sizeof (char *), &loc);
        }
      }
      free (fname);
    }

    mkc_dir_close (dh);
    free (dir);
  }
  mkc_list_free (dirqueue);

  return filelist;
}

