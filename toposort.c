/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mkc_def.h"
#include "mkc_error.h"
#include "list.h"
#include "strutil.h"
#include "toposort.h"

enum {
  MKC_TOPO_DONE = -1,
};

typedef struct mkc_topoitem_t {
  const char  * name;
} mkc_topoitem_t;

typedef struct mkc_topopair_t {
  listidx_t itemidx;
  listidx_t dependson;
} mkc_topopair_t;

typedef struct mkc_topocount_t {
  listidx_t idx;
  int           count;
} mkc_topocount_t;

typedef struct toposort_t {
  list_t    *items;
  list_t    *pairs;
  list_t    *counts;
  list_t    *results;
  mkc_error_t   *mkcerr;
  listidx_t riteridx;
} toposort_t;

static int mkc_topo_item_compare (void *ta, void *tb);
static int mkc_topo_count_compare (void *ta, void *tb);
static void mkc_topo_update_counts (toposort_t *topo, listidx_t idx);

toposort_t *
toposort_init (mkc_error_t *mkcerr)
{
  toposort_t  *topo;

  topo = malloc (sizeof (toposort_t));
  if (topo == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
  }

  topo->mkcerr = mkcerr;
  topo->items = list_init (MKC_LIST_SORTED, NULL, mkc_topo_item_compare,
      sizeof (mkc_topoitem_t), mkcerr);
  topo->pairs = list_init (MKC_LIST_UNSORTED, NULL, NULL,
      sizeof (mkc_topopair_t), mkcerr);
  topo->counts = list_init (MKC_LIST_SORTED, NULL, mkc_topo_count_compare,
      sizeof (mkc_topocount_t), mkcerr);
  topo->results = list_init (MKC_LIST_UNSORTED, NULL, NULL,
      sizeof (listidx_t), mkcerr);

  return topo;
}

void
toposort_free (toposort_t *topo)
{
  if (topo == NULL) {
    return;
  }

  list_free (topo->items);
  list_free (topo->pairs);
  list_free (topo->counts);
  list_free (topo->results);
  free (topo);
}

void
toposort_add_item (toposort_t *topo, const char *item)
{
  mkc_topoitem_t  titem;

  if (topo == NULL) {
    return;
  }

  titem.name = item;
  list_set (topo->items, &titem);

  return;
}

int
toposort_add_pair (toposort_t *topo,
    const char *item_a, const char *item_b)
{
  mkc_topopair_t    tpair;
  mkc_topoitem_t    titem;

  if (topo == NULL) {
    return MKC_ERR_FAILURE;
  }

  titem.name = item_a;
  tpair.itemidx = list_find (topo->items, &titem);
  if (tpair.itemidx == MKC_LIST_NOTFOUND) {
    mkc_error_set (topo->mkcerr, MKC_ERR_ITEM_NOT_FOUND, 0, item_a);
    return MKC_ERR_FAILURE;
  }
  titem.name = item_b;
  tpair.dependson = list_find (topo->items, &titem);
  if (tpair.dependson == MKC_LIST_NOTFOUND) {
    mkc_error_set (topo->mkcerr, MKC_ERR_ITEM_NOT_FOUND, 0, item_b);
    return MKC_ERR_FAILURE;
  }

  list_set (topo->pairs, &tpair);
  return MKC_OK;
}

/* uses Kahn's method */
int
toposort (toposort_t *topo)
{
  listidx_t   iteridx;
  listidx_t   idx;
  bool            done = false;
  int32_t         itemcount = 0;
  int             rc = MKC_ERR_FAILURE;

  itemcount = list_size (topo->items);

  /* create the initial counts list */
  list_iter_start (topo->items, &iteridx);
  while ((idx = list_iter_next (topo->items, &iteridx)) != MKC_ITER_FINISH) {
    mkc_topocount_t   count;

    count.idx = idx;
    count.count = 0;
    list_set (topo->counts, &count);
  }

  /* each pair is "a depends on b" */
  list_iter_start (topo->pairs, &iteridx);
  while ((idx = list_iter_next (topo->pairs, &iteridx)) != MKC_ITER_FINISH) {
    mkc_topopair_t  *pair;
    listidx_t   cidx;
    mkc_topocount_t *count;

    pair = list_get_by_idx (topo->pairs, idx);
    cidx = list_find (topo->counts, &pair->dependson);
    count = list_get_by_idx (topo->counts, cidx);
    count->count += 1;
  }

  /* overall loop */
  while (! done) {
    listidx_t   citeridx;
    listidx_t   cidx;
    mkc_topocount_t *count;
    int             found = 0;

    /* locate any items with a count of 0, add them to the results */
    list_iter_start (topo->counts, &citeridx);
    while ((cidx = list_iter_next (topo->counts, &citeridx)) != MKC_ITER_FINISH) {
      count = list_get_by_idx (topo->counts, cidx);
      if (count->count == 0) {
        found += 1;
        count->count = MKC_TOPO_DONE;

        list_set (topo->results, &count->idx);

        /* update the edge counts for items that the item depends on */
        mkc_topo_update_counts (topo, count->idx);
      }
    } /* for each item in the counts list */

    if (found == 0) {
      /* no items were found */
      done = true;
    }

    if (list_size (topo->results) == itemcount) {
      done = true;
      rc = MKC_OK;
    }
  }

  if (rc == MKC_ERR_FAILURE) {
    char    tbuff [MKC_VNAME_MAX];

    toposort_disp_cycle (topo, tbuff, sizeof (tbuff));
    mkc_error_set (topo->mkcerr, MKC_ERR_DEPENDENCY_CYCLE, 0, tbuff);
  }

  return rc;
}

void
toposort_iter_start (toposort_t *topo)
{
  list_iter_start (topo->results, &topo->riteridx);
}

const char *
toposort_iter_next (toposort_t *topo)
{
  listidx_t   ridx;
  listidx_t   *iptr;
  listidx_t   iidx;
  mkc_topoitem_t  *item;

  ridx = list_iter_next (topo->results, &topo->riteridx);
  if (ridx == MKC_ITER_FINISH) {
    return NULL;
  }

  iptr = list_get_by_idx (topo->results, ridx);
  iidx = *iptr;
  item = list_get_by_idx (topo->items, iidx);


  return item->name;
}

const char *
toposort_iter_next_reverse (toposort_t *topo)
{
  listidx_t   ridx;
  listidx_t   *iptr;
  listidx_t   iidx;
  mkc_topoitem_t  *item;

  ridx = list_iter_next_reverse (topo->results, &topo->riteridx);
  if (ridx == MKC_ITER_FINISH) {
    return NULL;
  }

  iptr = list_get_by_idx (topo->results, ridx);
  iidx = *iptr;
  item = list_get_by_idx (topo->items, iidx);

  return item->name;
}

// ### this needs to be improved
void
toposort_disp_cycle (toposort_t *topo, char *buff, size_t sz)
{
  listidx_t   citeridx;
  listidx_t   cidx;
  char            *p = buff;

  *buff = '\0';
  list_iter_start (topo->counts, &citeridx);
  while ((cidx = list_iter_next (topo->counts, &citeridx)) != MKC_ITER_FINISH) {
    mkc_topocount_t   *count;

    count = list_get_by_idx (topo->counts, cidx);
    if (count->count == 1) {
      mkc_topoitem_t   *item;

      item = list_get_by_idx (topo->items, count->idx);
      if (*buff) {
        p = stpecpy (p, buff + sz, " : ");
      }
      p = stpecpy (p, buff + sz, item->name);
    }
  }
}

/* internal routines */

static int
mkc_topo_item_compare (void *ta, void *tb)
{
  mkc_topoitem_t  *a = ta;
  mkc_topoitem_t  *b = tb;

  return strcmp (a->name, b->name);
}

static int
mkc_topo_count_compare (void *ta, void *tb)
{
  mkc_topocount_t  *a = ta;
  mkc_topocount_t  *b = tb;

  if (a->idx < b->idx) {
    return -1;
  }
  if (a->idx > b->idx) {
    return 1;
  }
  return 0;
}

static void
mkc_topo_update_counts (toposort_t *topo, listidx_t idx)
{
  listidx_t   pairiteridx;
  listidx_t   pairidx;

  list_iter_start (topo->pairs, &pairiteridx);
  while ((pairidx = list_iter_next (topo->pairs, &pairiteridx)) != MKC_ITER_FINISH) {
    mkc_topopair_t    *pair;
    listidx_t     cidx;
    mkc_topocount_t   *count;

    pair = list_get_by_idx (topo->pairs, pairidx);
    if (pair->itemidx == idx) {
      cidx = list_find (topo->counts, &pair->dependson);
      count = list_get_by_idx (topo->counts, cidx);
      count->count -= 1;
    }
  }
}
