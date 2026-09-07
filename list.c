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
#include "list.h"
#include "strutil.h"

typedef struct list_t {
  /* create as a char * so that we can do arithmetic on it */
  char                  * data;
  /* only the indexes are sorted. */
  /* the indirection allows stable data indexes. */
  listidx_t         * idxsort;
  mkc_error_t           * mkcerr;
  list_free_t       freefunc;
  list_compare_t    compare;
  size_t                itemsz;
  listidx_t         allocsz;
  listidx_t         sz;
  listidx_t         idxsz;
  list_type_t       type;
} list_t;

static int list_binary_search (list_t *list, void *data, listidx_t *loc);

MKC_NODISCARD
list_t *
list_init (list_type_t type, list_free_t freefunc,
    list_compare_t compare, mkc_error_t *mkcerr)
{
  list_t  *list;

  if (type == MKC_LIST_SORTED && compare == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return NULL;
  }

  list = malloc (sizeof (list_t));
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

MKC_NODISCARD
list_t *
list_init_copy (list_t *list, list_free_t freefunc, mkc_error_t *mkcerr)
{
  list_t  *nlist;

  if (list == NULL) {
    return NULL;
  }

  nlist = list_init (list->type, freefunc, list->compare, list->mkcerr);
  return nlist;
}

void
list_free (void *tlist)
{
  list_t    * list = tlist;

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
  datafree (list->idxsort);
  free (list);
}

listidx_t
list_size (list_t *list)
{
  if (list == NULL) {
    return 0;
  }

  if (list->type == MKC_LIST_UNSORTED) {
    return list->sz;
  }

  return list->idxsz;
}

list_type_t
list_get_type (list_t *list)
{
  if (list == NULL) {
    return MKC_LIST_UNSORTED;
  }

  return list->type;
}

list_compare_t
list_get_compfunc (list_t *list)
{
  if (list == NULL) {
    return NULL;
  }

  return list->compare;
}

void *
list_set (list_t *list, void *data, size_t sz)
{
  int         rc = MKC_LIST_NOTFOUND;
  listidx_t   newloc;
  listidx_t   dataloc;

  if (list == NULL || data == NULL) {
    return NULL;
  }

  if (list->type == MKC_LIST_UNSORTED) {
    data = list_append (list, data, sz);
    return data;
  }

  newloc = 0;
  dataloc = list->sz;
  list->itemsz = sz;

  if (list->idxsz > 0) {
    rc = list_binary_search (list, data, &newloc);
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
        sizeof (listidx_t) * list->allocsz);
    if (list->idxsort == NULL) {
      return NULL;
    }
  }

  if (rc == MKC_LIST_NOTFOUND && newloc < list->idxsz) {
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

  return list->data + list->itemsz * dataloc;
}

/* an append to a sorted list will not update the sort-index */
/* the data is simply appended to the list */
void *
list_append (list_t *list, void *data, size_t sz)
{
  int             rc = MKC_LIST_NOTFOUND;
  listidx_t   dataloc;

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

  return list->data + list->itemsz * dataloc;
}

void
list_pop (list_t *list, listidx_t lidx)
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

/* doing a delete invalidates any list indexes */
void
list_delete (list_t *list, listidx_t lidx, size_t sz)
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
  for (listidx_t i = lidx; i < list->sz; ++i) {
    memcpy (list->data + list->itemsz * i,
        list->data + list->itemsz * (i + 1), sz);
  }

  if (list->type == MKC_LIST_SORTED) {
    listidx_t   iidx = -1;

    for (listidx_t i = 0; i < list->idxsz; ++i) {
      if (list->idxsort [i] == lidx) {
        iidx = i;
      }
      if (list->idxsort [i] > lidx) {
        list->idxsort [i] -= 1;
      }
    }

    if (iidx == -1) {
      return;
    }

    list->idxsz -= 1;
    for (listidx_t i = iidx; i < list->idxsz; ++i) {
      list->idxsort [i] = list->idxsort [i + 1];
    }
  }
}

listidx_t
list_find (list_t *list, void *data)
{
  int32_t         rc = -1;
  listidx_t   loc = MKC_LIST_NOTFOUND;

  if (list == NULL) {
    return MKC_LIST_NOTFOUND;
  }

  if (list->type == MKC_LIST_UNSORTED) {
    mkc_error_set (list->mkcerr, MKC_ERR_SEARCH_UNSORTED_LIST, 0, NULL);
    fprintf (stderr, "ERROR: searching an unsorted list\n");
    return MKC_LIST_NOTFOUND;
  }

  rc = list_binary_search (list, data, &loc);
  if (rc == MKC_LIST_FOUND) {
    /* return the data index */
    loc = list->idxsort [loc];
    return loc;
  }

  return rc;
}

void *
list_get_by_idx (list_t *list, listidx_t idx)
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
list_iter_start (list_t *list, listidx_t *iteridx)
{
  if (list == NULL || iteridx == NULL) {
    return;
  }

  *iteridx = MKC_ITER_FINISH;
}

listidx_t
list_iter_next (list_t *list, listidx_t *iteridx)
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

listidx_t
list_iter_next_reverse (list_t *list, listidx_t *iteridx)
{
  if (list == NULL || iteridx == NULL) {
    return MKC_ITER_FINISH;
  }

  if (*iteridx == MKC_ITER_FINISH) {
    if (list->type == MKC_LIST_UNSORTED) {
      *iteridx = list->sz - 1;
    }
    if (list->type == MKC_LIST_SORTED) {
      *iteridx = list->idxsz - 1;
    }
    return *iteridx;
  }

  *iteridx -= 1;
  if (*iteridx < 0) {
    *iteridx = MKC_ITER_FINISH;
    return MKC_ITER_FINISH;
  }
  if (list->type == MKC_LIST_UNSORTED) {
    return *iteridx;
  }

  return list->idxsort [*iteridx];
}

void
list_ind_free (void *tdata)
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

int
list_ind_compare (void *ta, void *tb)
{
  char    **ia = ta;
  char    **ib = tb;
  char    *a;
  char    *b;

  if (ia == NULL || ib == NULL) {
    return 0;
  }

  a = *ia;
  b = *ib;

  return strcmp (a, b);
}

/* internal routines */

/* loc points to the idxsort entry, not the dataidx */
static int
list_binary_search (list_t *list,
    void *data, listidx_t *loc)
{
  listidx_t   l = 0;
  listidx_t   r = list->idxsz - 1;
  listidx_t   m = 0;
  listidx_t   rm;
  int             rc;

  rm = 0;
  while (l <= r) {
    listidx_t   dataidx;

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

