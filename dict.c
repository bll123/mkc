/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "dict.h"
#include "mkc_def.h"
#include "mkc_error.h"
#include "list.h"
#include "mkc_log.h"
#include "strutil.h"

typedef struct dictitem_t {
  char    * name;
  void    * data;
  dict_t  * dict;
} dictitem_t;

typedef struct dict_t {
  list_t          * list;
  mkc_log_t       * log;
  mkc_error_t     * mkcerr;
  list_free_t     freefunc;
  size_t          itemsz;
} dict_t;

static void dictitem_free (void *titema);
static int dictitem_compare (void *titema, void *titemb);

dict_t *
dict_init (mkc_log_t *log, list_free_t freefunc,
    size_t sz, mkc_error_t *mkcerr)
{
  dict_t    * dict;

  dict = malloc (sizeof (dict_t));
  if (dict == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  dict->list = list_init (MKC_LIST_SORTED,
      dictitem_free, dictitem_compare, mkcerr);
  dict->log = log;
  dict->mkcerr = mkcerr;
  dict->freefunc = freefunc;
  dict->itemsz = sz;

  return dict;
}

MKC_NODISCARD dict_t *
dict_init_copy (dict_t *dict, list_free_t freefunc)
{
  dict_t    * ndict;

  ndict = dict_init (dict->log, freefunc, dict->itemsz, dict->mkcerr);
  return ndict;
}

void
dict_free (dict_t * dict)
{
  if (dict == NULL) {
    return;
  }

  list_free (dict->list);
  free (dict);
}

listidx_t
dict_size (dict_t * dict)
{
  if (dict == NULL) {
    return 0;
  }

  return list_size (dict->list);
}

void
dict_set (dict_t * dict, const char * name, void *data)
{
  dictitem_t    ditem;

  if (name == NULL) {
    mkc_error_set (dict->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  ditem.data = malloc (dict->itemsz);
  if (ditem.data == NULL) {
    mkc_error_set (dict->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }
  memcpy (ditem.data, data, dict->itemsz);

  ditem.name = strdup (name);
  if (ditem.name == NULL) {
    mkc_error_set (dict->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (ditem.data);
    return;
  }
  ditem.dict = dict;

  list_set (dict->list, &ditem, sizeof (dictitem_t));
fprintf (stderr, "dict: set %s ok\n", ditem.name);
}

void *
dict_get (dict_t * dict, const char * name)
{
  dictitem_t    titem;
  dictitem_t    * ditem = NULL;
  listidx_t idx;

  if (dict == NULL) {
    return NULL;
  }

  titem.name = (char *) name;

  idx = list_find (dict->list, &titem);
fprintf (stderr, "dict: find: %d\n", idx);
  if (idx != MKC_LIST_NOTFOUND) {
    ditem = list_get_by_idx (dict->list, idx);
  }

  return ditem->data;
}

void
dict_iter_start (dict_t *dict, listidx_t *iteridx)
{
  if (dict == NULL || iteridx == NULL) {
    return;
  }

  list_iter_start (dict->list, iteridx);
  return;
}

dictitem_t *
dict_iter_next (dict_t *dict, listidx_t *iteridx)
{
  dictitem_t    * ditem = NULL;
  listidx_t     lidx;

  if (dict == NULL) {
    return NULL;
  }

  lidx = list_iter_next (dict->list, iteridx);
  if (lidx == MKC_ITER_FINISH) {
    return NULL;
  }
  ditem = list_get_by_idx (dict->list, lidx);
  return ditem;
}

const char *
dict_iter_get_name (dictitem_t *ditem)
{
  if (ditem == NULL) {
    return NULL;
  }

  return ditem->name;
}

void *
dict_iter_get_data (dictitem_t *ditem)
{
  if (ditem == NULL) {
    return NULL;
  }

  return ditem->data;
}

/* internal routines */

static void
dictitem_free (void *titema)
{
  dictitem_t   * ditem = titema;

  if (ditem == NULL) {
    return;
  }

  datafree (ditem->name);
  ditem->name = NULL;
  if (ditem->dict->freefunc != NULL && ditem->data != NULL) {
    (*ditem->dict->freefunc) (ditem->data);
  }
  datafree (ditem->data);
  ditem->data = NULL;
}

static int
dictitem_compare (void *titema, void *titemb)
{
  dictitem_t  * ditema = titema;
  dictitem_t  * ditemb = titemb;
  int         rc;


  if (ditema == NULL || ditemb == NULL) {
fprintf (stderr, "d: comp: null\n");
    return 0;
  }

fprintf (stderr, "d: comp: %s %s\n", ditema->name, ditemb->name);
  rc = strcmp (ditema->name, ditemb->name);
  return rc;
}

