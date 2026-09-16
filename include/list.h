/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_LIST_H
#define INC_LIST_H

#include <stddef.h>
#include <stdint.h>

#include "mkc_error.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  MKC_LIST_SORTED,
  MKC_LIST_UNSORTED,
} list_type_t;

enum {
  MKC_LIST_FOUND = -1,
  MKC_LIST_NOTFOUND = -2,
  MKC_ITER_FINISH = -1,
};

typedef struct list_t list_t;
typedef int32_t listidx_t;

typedef void (*list_free_t) (void *data);
typedef int (*list_compare_t) (void *ditema, void *ditemb);

MKC_NODISCARD list_t * list_init (list_type_t type, list_free_t freefunc, list_compare_t compare, size_t itemsz, mkc_error_t *mkcerr);
MKC_NODISCARD list_t * list_init_copy (list_t *list, list_free_t freefunc, mkc_error_t *mkcerr);
void list_free (void *list);
listidx_t list_size (list_t *list);
list_type_t list_get_type (list_t *list);
list_compare_t list_get_compfunc (list_t *list);

void * list_set (list_t *list, void *data);
void * list_append (list_t *list, void *data);
void list_pop (list_t *list, listidx_t lidx);
void list_delete (list_t *list, listidx_t lidx);

listidx_t list_find (list_t *list, void *data);
void * list_get_by_idx (list_t *list, listidx_t idx);

void list_iter_start (list_t *list, listidx_t *iteridx);
listidx_t list_iter_next (list_t *list, listidx_t *iteridx);
listidx_t list_iter_next_reverse (list_t *list, listidx_t *iteridx);

void list_ind_free (void *data);
int list_ind_compare (void *a, void *b);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_LIST_H */
