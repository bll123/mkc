/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_DICT_H
#define INC_DICT_H

#include <stddef.h>
#include <stdint.h>

#include "mkc_error.h"
#include "list.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct dict_t dict_t;
typedef struct dictitem_t dictitem_t;

MKC_NODISCARD dict_t * dict_init (mkc_log_t *log, list_free_t freefunc, size_t sz, mkc_error_t *mkcerr);
MKC_NODISCARD dict_t * dict_init_copy (dict_t *dict, list_free_t freefunc);
void dict_free (dict_t *dict);
listidx_t dict_size (dict_t *dict);

listidx_t dict_size (dict_t *dict);

void dict_set (dict_t *dict, const char *name, void *data);

void * dict_get (dict_t *dict, const char *name);

void dict_iter_start (dict_t *dict, listidx_t *iteridx);
dictitem_t * dict_iter_next (dict_t *dict, listidx_t *iteridx);
const char * dict_iter_get_name (dictitem_t *ditem);
void * dict_iter_get_data (dictitem_t *ditem);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_DICT_H */
