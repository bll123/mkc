/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "chararr.h"
#include "mkc_error.h"
#include "strutil.h"

typedef struct chararr_t {
  mkc_error_t *mkcerr;
  char        ** targv;
  int         sz;
  int         allocsz;
  bool        freeinternals;
} chararr_t;

chararr_t *
chararr_init (mkc_error_t *mkcerr)
{
  chararr_t   * carr;

  carr = malloc (sizeof (chararr_t));
  if (carr == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  carr->mkcerr = mkcerr;
  carr->allocsz = 0;
  carr->sz = 0;
  carr->targv = NULL;
  carr->freeinternals = false;

  return carr;
}

void
chararr_free (chararr_t *carr)
{
  if (carr == NULL) {
    return;
  }

  if (carr->freeinternals) {
    for (int i = 0; i < carr->sz; ++i) {
      free (carr->targv [i]);
    }
  }
  datafree (carr->targv);
  free (carr);
}

int
chararr_size (chararr_t *carr)
{
  if (carr == NULL) {
    return 0;
  }

  return carr->sz;
}

void
chararr_reset (chararr_t *carr, int idx)
{
  if (carr == NULL) {
    return;
  }

  if (carr->freeinternals) {
    /* doesn't make sense to do a reset if the internals need to be freed */
    return;
  }

  carr->sz = idx;
  if (carr->targv != NULL) {
    carr->targv [carr->sz] = NULL;
  }
}

void
chararr_append (chararr_t *carr, const char *txt)
{
  if (carr == NULL) {
    return;
  }

  if (carr->sz >= carr->allocsz) {
    carr->allocsz += 10;
    carr->targv = realloc (carr->targv,
        carr->allocsz * sizeof (const char *));
    if (carr->targv == NULL) {
      mkc_error_set (carr->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return;
    }
  }

  carr->targv [carr->sz] = (char *) txt;
  carr->sz += 1;
}

void
chararr_freeinternals (chararr_t *carr)
{
  if (carr == NULL) {
    return;
  }

  carr->freeinternals = true;
}

const char **
chararr_get_arr (chararr_t *carr)
{
  if (carr == NULL) {
    return NULL;
  }

  if (carr->sz == 0) {
    return NULL;
  }

  return (const char **) carr->targv;
}

