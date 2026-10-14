/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_nodiscard.h"
#include "tmppath.h"

typedef struct tmppath_t {
  char          ** path;
  bool          * avail;
  mkc_error_t   * mkcerr;
  int           allocsz;
  int           inuse;
  int           maxinuse;
} tmppath_t;

MKC_NODISCARD
tmppath_t *
tmppath_init (mkc_error_t *mkcerr, int initialalloc)
{
  tmppath_t   *tp;

  if (initialalloc <= 0) {
    initialalloc = 5;
  }

  tp = malloc (sizeof (tmppath_t));
  tp->allocsz = initialalloc;
  tp->path = malloc (sizeof (char *) * tp->allocsz);
  tp->avail = malloc (sizeof (bool) * tp->allocsz);
  for (int i = 0; i < tp->allocsz; ++i) {
    tp->path [i] = malloc (MKC_PATH_MAX);
    tp->avail [i] = true;
  }
  tp->inuse = 0;
  tp->maxinuse = 0;
  tp->mkcerr = mkcerr;

  return tp;
}

void
tmppath_free (tmppath_t *tp)
{
  for (int i = 0; i < tp->allocsz; ++i) {
    free (tp->path [i]);
  }
  free (tp->path);
  free (tp->avail);
  free (tp);
}

char *
tmppath_get (tmppath_t *tp)
{
  int     idx = -1;

  for (int i = 0; i < tp->allocsz; ++i) {
    if (tp->avail [i]) {
      idx = i;
      break;
    }
  }

  if (idx < 0) {
    int     lastsz = tp->allocsz;

    tp->allocsz += 5;
    tp->path = realloc (tp->path, sizeof (char *) * tp->allocsz);
    if (tp->path == NULL) {
      mkc_error_set (tp->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return NULL;
    }
    tp->avail = realloc (tp->avail, sizeof (bool) * tp->allocsz);
    if (tp->avail == NULL) {
      mkc_error_set (tp->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return NULL;
    }
    for (int i = lastsz; i < tp->allocsz; ++i) {
      tp->path [i] = malloc (MKC_PATH_MAX);
      tp->avail [i] = true;
    }

    idx = lastsz;
  }

  tp->inuse += 1;
  if (tp->inuse > tp->maxinuse) {
    tp->maxinuse = tp->inuse;
  }
  tp->avail [idx] = false;
  return tp->path [idx];
}

void
tmppath_release (tmppath_t *tp, char *path)
{
  for (int i = 0; i < tp->allocsz; ++i) {
    if (path == tp->path [i]) {
      tp->inuse -= 1;
      tp->avail [i] = true;
      break;
    }
  }
}
