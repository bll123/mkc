/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_TOPO_H
#define INC_MKC_TOPO_H

#include "mkc_list.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct toposort_t toposort_t;

MKC_NODISCARD toposort_t * toposort_init (mkc_error_t *mkcerr);
void toposort_free (toposort_t *topo);
void toposort_add_item (toposort_t *topo, const char *item);
int toposort_add_pair (toposort_t *topo, const char *item_a, const char *item_b);
int toposort (toposort_t *topo);
void toposort_iter_start (toposort_t *topo);
const char *toposort_iter_next (toposort_t *topo);
void toposort_disp_cycle (toposort_t *topo, char *buff, size_t sz);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_TOPO_H */
