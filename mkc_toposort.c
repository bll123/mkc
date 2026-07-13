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
#include "mkc_list.h"
#include "mkc_string.h"
#include "mkc_toposort.h"

enum {
  MKC_TOPO_DONE = -1,
};

typedef struct mkc_topoitem_t {
  char  *name;
} mkc_topoitem_t;

typedef struct mkc_topopair_t {
  mkc_listidx_t itemidx;
  mkc_listidx_t dependson;
} mkc_topopair_t;

typedef struct mkc_topocount_t {
  mkc_listidx_t idx;
  int           count;
} mkc_topocount_t;

typedef struct mkc_toposort_t {
  mkc_list_t    *items;
  mkc_list_t    *pairs;
  mkc_list_t    *counts;
  mkc_list_t    *results;
  mkc_error_t   *mkcerr;
  mkc_listidx_t riteridx;
} mkc_toposort_t;

static int mkc_topo_item_compare (void *ta, void *tb);
static int mkc_topo_count_compare (void *ta, void *tb);
static void mkc_topo_update_counts (mkc_toposort_t *topo, mkc_listidx_t idx);
static void mkc_topo_item_free (void *titem);

mkc_toposort_t *
mkc_toposort_init (mkc_error_t *mkcerr)
{
  mkc_toposort_t  *topo;

  topo = malloc (sizeof (mkc_toposort_t));
  if (topo == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
  }

  topo->mkcerr = mkcerr;
  topo->items = mkc_list_init (MKC_LIST_SORTED, mkc_topo_item_free, mkc_topo_item_compare, mkcerr);
  topo->pairs = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, mkcerr);
  topo->counts = mkc_list_init (MKC_LIST_SORTED, NULL, mkc_topo_count_compare, mkcerr);
  topo->results = mkc_list_init (MKC_LIST_UNSORTED, NULL, NULL, mkcerr);

  return topo;
}

void
mkc_toposort_free (mkc_toposort_t *topo)
{
  if (topo == NULL) {
    return;
  }

  mkc_list_free (topo->items);
  mkc_list_free (topo->pairs);
  mkc_list_free (topo->counts);
  mkc_list_free (topo->results);
  free (topo);
}

void
mkc_toposort_add_item (mkc_toposort_t *topo, const char *item)
{
  mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
  mkc_topocount_t count;
  mkc_topoitem_t  titem;

  if (topo == NULL) {
    return;
  }

  titem.name = strdup (item);
  mkc_list_set (topo->items, &titem, sizeof (mkc_topoitem_t), &loc);

  count.idx = loc;
  count.count = 0;
  loc = MKC_LIST_NOTFOUND;
  mkc_list_set (topo->counts, &count, sizeof (mkc_topocount_t), &loc);

  return;
}

int
mkc_toposort_add_pair (mkc_toposort_t *topo,
    const char *item_a, const char *item_b)
{
  mkc_topopair_t    tpair;
  mkc_listidx_t     loc = MKC_LIST_NOTFOUND;
  mkc_topoitem_t    titem;

  if (topo == NULL) {
    return MKC_ERR_FAILURE;
  }

  titem.name = (char *) item_a;
  tpair.itemidx = mkc_list_find (topo->items, &titem, &loc);
  if (tpair.itemidx == MKC_LIST_NOTFOUND) {
    mkc_error_set (topo->mkcerr, MKC_ERR_FILE_NOT_FOUND, 0, item_a);
    return MKC_ERR_FAILURE;
  }
  titem.name = (char *) item_b;
  tpair.dependson = mkc_list_find (topo->items, &titem, &loc);
  if (tpair.dependson == MKC_LIST_NOTFOUND) {
    mkc_error_set (topo->mkcerr, MKC_ERR_FILE_NOT_FOUND, 0, item_b);
    return MKC_ERR_FAILURE;
  }

  mkc_list_set (topo->pairs, &tpair, sizeof (mkc_topopair_t), &loc);

  return MKC_OK;
}

/* uses Kahn's method */
int
mkc_toposort (mkc_toposort_t *topo)
{
  mkc_listidx_t   pairiteridx;
  mkc_listidx_t   pairidx;
  bool            done = false;
  int32_t         itemcount = 0;
  int             rc = MKC_ERR_FAILURE;

  itemcount = mkc_list_size (topo->items);

  /* populate the initial counts list */
  /* each pair is "a depends on b" */
  mkc_list_iter_start (topo->pairs, &pairiteridx);
  while ((pairidx = mkc_list_iter_next (topo->pairs, &pairiteridx)) != MKC_ITER_FINISH) {
    mkc_topopair_t  *pair;
    mkc_listidx_t   loc;
    mkc_listidx_t   cidx;
    mkc_topocount_t *count;

    pair = mkc_list_get_by_idx (topo->pairs, pairidx);
    cidx = mkc_list_find (topo->counts, &pair->dependson, &loc);
    count = mkc_list_get_by_idx (topo->counts, cidx);
    count->count += 1;
  }

  /* overall loop */
  while (! done) {
    mkc_listidx_t   citeridx;
    mkc_listidx_t   cidx;
    mkc_topocount_t *count;
    mkc_listidx_t   loc = MKC_LIST_NOTFOUND;
    int             found = 0;

    /* locate any items with a count of 0, add them to the results */
    mkc_list_iter_start (topo->counts, &citeridx);
    while ((cidx = mkc_list_iter_next (topo->counts, &citeridx)) != MKC_ITER_FINISH) {
      count = mkc_list_get_by_idx (topo->counts, cidx);
      if (count->count == 0) {
        found += 1;
        count->count = MKC_TOPO_DONE;

        loc = MKC_LIST_NOTFOUND;
        mkc_list_set (topo->results, &count->idx, sizeof (mkc_listidx_t), &loc);

        /* update the edge counts for items that the item depends on */
        mkc_topo_update_counts (topo, count->idx);
      }
    } /* for each item in the counts list */

    if (found == 0) {
      /* no items were found */
      done = true;
    }

    if (mkc_list_size (topo->results) == itemcount) {
      done = true;
      rc = MKC_OK;
    }
  }

  return rc;
}

void
mkc_toposort_iter_start (mkc_toposort_t *topo)
{
  mkc_list_iter_start (topo->results, &topo->riteridx);
}

const char *
mkc_toposort_iter_next (mkc_toposort_t *topo)
{
  mkc_listidx_t   ridx;
  mkc_listidx_t   *iptr;
  mkc_listidx_t   iidx;
  mkc_topoitem_t  *item;

  ridx = mkc_list_iter_next (topo->results, &topo->riteridx);
  if (ridx == MKC_ITER_FINISH) {
    return NULL;
  }

  iptr = mkc_list_get_by_idx (topo->results, ridx);
  iidx = *iptr;
  item = mkc_list_get_by_idx (topo->items, iidx);

  return item->name;
}

// ### this needs to be improved
void
mkc_toposort_disp_cycle (mkc_toposort_t *topo, char *buff, size_t sz)
{
  mkc_listidx_t   citeridx;
  mkc_listidx_t   cidx;
  char            *p = buff;

  *buff = '\0';
  mkc_list_iter_start (topo->counts, &citeridx);
  while ((cidx = mkc_list_iter_next (topo->counts, &citeridx)) != MKC_ITER_FINISH) {
    mkc_topocount_t   *count;

    count = mkc_list_get_by_idx (topo->counts, cidx);
    if (count->count == 1) {
      mkc_topoitem_t   *item;

      item = mkc_list_get_by_idx (topo->items, count->idx);
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
mkc_topo_update_counts (mkc_toposort_t *topo, mkc_listidx_t idx)
{
  mkc_listidx_t   pairiteridx;
  mkc_listidx_t   pairidx;

  mkc_list_iter_start (topo->pairs, &pairiteridx);
  while ((pairidx = mkc_list_iter_next (topo->pairs, &pairiteridx)) != MKC_ITER_FINISH) {
    mkc_topopair_t    *pair;
    mkc_listidx_t     cidx;
    mkc_topocount_t   *count;
    mkc_listidx_t     loc;

    pair = mkc_list_get_by_idx (topo->pairs, pairidx);
    if (pair->itemidx == idx) {
      cidx = mkc_list_find (topo->counts, &pair->dependson, &loc);
      count = mkc_list_get_by_idx (topo->counts, cidx);
      count->count -= 1;
    }
  }
}

static void
mkc_topo_item_free (void *titem)
{
  mkc_topoitem_t    *item = titem;

  free (item->name);
}
