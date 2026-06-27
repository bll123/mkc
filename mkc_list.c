/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_string.h"

typedef struct mkc_list_t {
  /* create as a char * so that we can do arithmetic on it */
  char                  * data;
  /* only the indexes are sorted. */
  /* the indirection allows stable data indexes. */
  mkc_listidx_t         * idxsort;
  mkc_error_t           * mkcerr;
  mkc_list_free_t       freefunc;
  mkc_list_compare_t    compare;
  size_t                itemsz;
  mkc_listidx_t         allocsz;
  mkc_listidx_t         sz;
  mkc_listidx_t         idxsz;
  mkc_list_type_t       type;
} mkc_list_t;

static int mkc_list_binary_search (mkc_list_t *list, void *data, mkc_listidx_t *loc);

MKC_NODISCARD
mkc_list_t *
mkc_list_init (mkc_list_type_t type, mkc_list_free_t freefunc,
    mkc_list_compare_t compare, mkc_error_t *mkcerr)
{
  mkc_list_t  *list;

  if (type == MKC_LIST_SORTED && compare == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

  list = malloc (sizeof (mkc_list_t));
  if (list == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  list->data = NULL;
  list->idxsort = NULL;
  list->freefunc = freefunc;
  list->compare = compare;
  list->itemsz = 0;
  list->allocsz = 0;
  list->sz = 0;
  list->idxsz = 0;
  list->type = type;
  list->mkcerr = mkcerr;

  return list;
}

void
mkc_list_free (mkc_list_t *list)
{
  if (list == NULL) {
    return;
  }

  if (list->data != NULL) {
    if (list->freefunc != NULL) {
      for (int i = 0; i < list->sz; ++i) {
        char    *data;

        data = list->data + list->itemsz * i;
        (*list->freefunc) (data);
      }
    }
    free (list->data);
  }
  if (list->idxsort != NULL) {
    free (list->idxsort);
  }

  free (list);
}

mkc_listidx_t
mkc_list_size (mkc_list_t *list)
{
  if (list == NULL) {
    return 0;
  }

  if (list->type == MKC_LIST_UNSORTED) {
    return list->sz;
  }

  return list->idxsz;
}

void *
mkc_list_set (mkc_list_t *list, void *data, size_t sz, mkc_listidx_t *loc)
{
  int             rc = MKC_LIST_NOTFOUND;
  mkc_listidx_t   newloc;
  mkc_listidx_t   dataloc;

  if (list == NULL || data == NULL) {
    return NULL;
  }

  if (list->type == MKC_LIST_UNSORTED) {
    data = mkc_list_append (list, data, sz, loc);
    return data;
  }

  newloc = 0;
  dataloc = list->sz;
  list->itemsz = sz;

  if (*loc != MKC_LIST_NOTFOUND) {
    newloc = *loc;
  } else {
    if (list->idxsz > 0) {
      rc = mkc_list_binary_search (list, data, &newloc);
    }
  }

  if (rc == MKC_LIST_FOUND) {
    dataloc = list->idxsort [newloc];
  }

  if (rc == MKC_LIST_NOTFOUND &&
      list->allocsz <= list->sz) {
    list->allocsz += 10;
    list->data = realloc (list->data, list->itemsz * list->allocsz);
    if (list->data == NULL) {
      return NULL;
    }
    list->idxsort = realloc (list->idxsort,
        sizeof (mkc_listidx_t) * list->allocsz);
    if (list->idxsort == NULL) {
      return NULL;
    }
  }

  if (rc == MKC_LIST_NOTFOUND &&
      newloc < list->idxsz) {
    for (int i = list->idxsz; i > newloc; --i) {
      if (i > 0) {
        list->idxsort [i] = list->idxsort [i - 1];
      }
    }
  }

  if (rc == MKC_LIST_NOTFOUND) {
    /* the data is always added at the end... */
    memcpy (list->data + list->itemsz * dataloc, data, sz);
    list->idxsort [newloc] = dataloc;
    list->sz += 1;
    list->idxsz += 1;
  }

  if (loc != NULL) {
    /* return the data index, not the idx-sort index */
    *loc = dataloc;
  }

  return list->data + list->itemsz * dataloc;
}

/* an append to a sorted list will not update the sort-index */
/* the data is simply appended to the list */
void *
mkc_list_append (mkc_list_t *list, void *data, size_t sz, mkc_listidx_t *loc)
{
  int             rc = MKC_LIST_NOTFOUND;
  mkc_listidx_t   dataloc;

  if (list == NULL || data == NULL) {
    return NULL;
  }

  dataloc = list->sz;
  list->itemsz = sz;

  if (rc == MKC_LIST_NOTFOUND &&
      list->allocsz <= list->sz) {
    list->allocsz += 10;
    list->data = realloc (list->data, list->itemsz * list->allocsz);
    if (list->data == NULL) {
      mkc_error_set (list->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return NULL;
    }
  }

  memcpy (list->data + list->itemsz * dataloc, data, sz);
  list->sz += 1;
  *loc = dataloc;

  return list->data + list->itemsz * dataloc;
}

void
mkc_list_pop (mkc_list_t *list, mkc_listidx_t lidx)
{
  void *data;

  if (list == NULL || lidx >= list->sz) {
    return;
  }

  if (list->freefunc != NULL) {
    data = list->data + list->itemsz * lidx;
    (*list->freefunc) (data);
  }

  list->sz -= 1;
}

mkc_listidx_t
mkc_list_find (mkc_list_t *list, void *data, mkc_listidx_t *loc)
{
  int32_t         rc = -1;

  if (list == NULL) {
    return *loc;
  }

  if (list->type == MKC_LIST_UNSORTED) {
    mkc_error_set (list->mkcerr, MKC_ERR_SEARCH_UNSORTED_LIST, 0, NULL);
    fprintf (stderr, "ERROR: searching an unsorted list\n");
    return *loc;
  }

  rc = mkc_list_binary_search (list, data, loc);
  if (rc == MKC_LIST_FOUND) {
    /* return the data index */
    *loc = list->idxsort [*loc];
    return *loc;
  }

  return rc;
}

void *
mkc_list_get_by_idx (mkc_list_t *list, mkc_listidx_t idx)
{
  void      *data;

  if (list == NULL) {
    return NULL;
  }

  if (idx < 0 || idx >= list->sz) {
    mkc_error_set (list->mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return NULL;
  }

  data = list->data + list->itemsz * idx;
  return data;
}

void
mkc_list_iter_start (mkc_list_t *list, mkc_listidx_t *iteridx)
{
  if (list == NULL || iteridx == NULL) {
    return;
  }

  *iteridx = MKC_ITER_FINISH;
}

mkc_listidx_t
mkc_list_iter_next (mkc_list_t *list, mkc_listidx_t *iteridx)
{
  if (list == NULL || iteridx == NULL) {
    return MKC_ITER_FINISH;
  }

  *iteridx += 1;
  if (list->type == MKC_LIST_UNSORTED) {
    if (*iteridx >= list->sz) {
      *iteridx = MKC_ITER_FINISH;
      return MKC_ITER_FINISH;
    }

    return *iteridx;
  }

  /* list is sorted, use the index-size */
  if (*iteridx >= list->idxsz) {
    *iteridx = MKC_ITER_FINISH;
    return MKC_ITER_FINISH;
  }

  return list->idxsort [*iteridx];
}

void
mkc_list_ind_free (void *tdata)
{
  char    **data = tdata;
  char    *tmp;

  if (data == NULL) {
    return;
  }

  tmp = *data;

  if (tmp != NULL) {
    free (tmp);
  }
}

/* internal routines */

/* loc points to the idxsort entry, not the dataidx */
static int
mkc_list_binary_search (mkc_list_t *list,
    void *data, mkc_listidx_t *loc)
{
  mkc_listidx_t   l = 0;
  mkc_listidx_t   r = list->idxsz - 1;
  mkc_listidx_t   m = 0;
  mkc_listidx_t   rm;
  int             rc;

  rm = 0;
  while (l <= r) {
    mkc_listidx_t   dataidx;

    m = l + (r - l) / 2;

    dataidx = list->idxsort [m];
    rc = (*list->compare) (list->data + list->itemsz * dataidx, data);
    if (rc == 0) {
      *loc = m;
      return MKC_LIST_FOUND;
    }

    if (rc < 0) {
      l = m + 1;
      rm = l;
    } else {
      r = m - 1;
      rm = m;
    }
  }

  *loc = rm;
  return MKC_LIST_NOTFOUND;
}

