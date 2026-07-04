/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_LIST_H
#define INC_MKC_LIST_H

#include <stddef.h>
#include <stdint.h>

#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  MKC_LIST_SORTED,
  MKC_LIST_UNSORTED,
} mkc_list_type_t;

enum {
  MKC_LIST_FOUND = -1,
  MKC_LIST_NOTFOUND = -2,
  MKC_ITER_FINISH = -1,
};

typedef struct mkc_list_t mkc_list_t;
typedef int32_t mkc_listidx_t;

typedef void (*mkc_list_free_t) (void *data);
typedef int (*mkc_list_compare_t) (void *ditema, void *ditemb);

MKC_NODISCARD mkc_list_t * mkc_list_init (mkc_list_type_t type, mkc_list_free_t freefunc, mkc_list_compare_t compare, mkc_error_t *mkcerr);
void mkc_list_free (mkc_list_t *list);
mkc_listidx_t mkc_list_size (mkc_list_t *list);
void * mkc_list_set (mkc_list_t *list, void *data, size_t sz, mkc_listidx_t *loc);
void * mkc_list_append (mkc_list_t *list, void *data, size_t sz);
void mkc_list_pop (mkc_list_t *list, mkc_listidx_t lidx);
mkc_listidx_t mkc_list_find (mkc_list_t *list, void *data, mkc_listidx_t *loc);
void * mkc_list_get_by_idx (mkc_list_t *list, mkc_listidx_t idx);
void mkc_list_iter_start (mkc_list_t *list, mkc_listidx_t *iteridx);
mkc_listidx_t mkc_list_iter_next (mkc_list_t *list, mkc_listidx_t *iteridx);
void mkc_list_ind_free (void *data);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_LIST_H */
