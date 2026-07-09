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

typedef struct mkc_toposort_t mkc_toposort_t;

MKC_NODISCARD mkc_toposort_t * mkc_toposort_init (mkc_error_t *mkcerr);
void mkc_toposort_free (mkc_toposort_t *topo);
void mkc_toposort_add_item (mkc_toposort_t *topo, const char *item);
int mkc_toposort_add_pair (mkc_toposort_t *topo, const char *item_a, const char *item_b);
int mkc_toposort (mkc_toposort_t *topo);
void mkc_toposort_iter_start (mkc_toposort_t *topo);
const char *mkc_toposort_iter_next (mkc_toposort_t *topo);
void mkc_toposort_disp_cycle (mkc_toposort_t *topo, char *buff, size_t sz);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_TOPO_H */
