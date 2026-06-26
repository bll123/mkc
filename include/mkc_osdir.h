/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *    (from ballroomdj4)
 */
#ifndef INC_MKC_OSDIR_H
#define INC_MKC_OSDIR_H

#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct dirhandle dirhandle_t;

MKC_NODISCARD dirhandle_t   * mkc_osdir_open (const char *dir);
MKC_NODISCARD char          * mkc_osdir_iterate (dirhandle_t *dirh);
void          mkc_osdir_close (dirhandle_t *dirh);

#if defined (__cplusplus) || defined (c_plusplus)
} /* extern C */
#endif

#endif /* INC_MKC_OSDIR_H */
