/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "parsefn.h"
#include "strutil.h"

typedef struct parse_file_t {
  char        * filename;
} parse_file_t;

typedef struct parsefn_t {
  mkc_error_t         * mkcerr;
  mkc_log_t           * log;
  parse_file_t        * parsefile;
  int                 pfsize;
  int                 pfallocsz;
} parsefn_t;

MKC_NODISCARD
parsefn_t *
parsefn_init (mkc_log_t *log, mkc_error_t *mkcerr)
{
  parsefn_t   *parsefn;

  parsefn = malloc (sizeof (parsefn_t));
  if (parsefn == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  parsefn->log = log;
  parsefn->mkcerr = mkcerr;
  parsefn->pfallocsz = 0;
  parsefn->pfsize = 0;
  parsefn->parsefile = NULL;

  return parsefn;
}

void
parsefn_free (parsefn_t *parsefn)
{
  if (parsefn == NULL) {
    return;
  }

  for (int i = 0; i < parsefn->pfsize; ++i) {
    datafree (parsefn->parsefile [i].filename);
  }
  datafree (parsefn->parsefile);
  free (parsefn);
}

void
parsefn_set (parsefn_t *parsefn, const char *fname)
{
  const char    * p;
  parse_file_t  * pf;
  int           idx;

  if (parsefn == NULL) {
    return;
  }

  idx = parsefn->pfsize;
  parsefn->pfsize += 1;
  if (parsefn->pfsize >= parsefn->pfallocsz) {
    parsefn->pfallocsz += 5;
    parsefn->parsefile = realloc (parsefn->parsefile,
        sizeof (parse_file_t) * parsefn->pfallocsz);
    if (parsefn->parsefile == NULL) {
      mkc_error_set (parsefn->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return;
    }
    for (int i = idx; i < parsefn->pfallocsz; ++i) {
      parsefn->parsefile [i].filename = NULL;
    }
  }

  pf = &parsefn->parsefile [idx];

  p = strrchr (fname, '/');
  if (p != NULL) {
    p += 1;
    pf->filename = strdup (p);
  } else {
    pf->filename = strdup (fname);
  }
  mkc_log_set_disp_filename (parsefn->log, pf->filename);
}

const char *
parsefn_get (parsefn_t *parsefn)
{
  parse_file_t  * pf;

  if (parsefn == NULL) {
    return NULL;
  }

  pf = &parsefn->parsefile [parsefn->pfsize - 1];
  return pf->filename;
}

void
parsefn_pop (parsefn_t *parsefn)
{
  parse_file_t    * pf;

  pf = &parsefn->parsefile [parsefn->pfsize - 1];
  datafree (pf->filename);
  parsefn->pfsize -= 1;
  if (parsefn->pfsize > 0) {
    pf = &parsefn->parsefile [parsefn->pfsize - 1];
    mkc_log_set_disp_filename (parsefn->log, pf->filename);
  }
  return;
}
