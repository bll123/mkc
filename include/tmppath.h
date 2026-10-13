/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_TMPPATH_H
#define INC_TMPPATH_H

#include "mkc_error.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct tmppath_t tmppath_t;

MKC_NODISCARD tmppath_t * tmppath_init (mkc_error_t *mkcerr, int initialalloc);
void tmppath_free (tmppath_t *tmppath);
char * tmppath_get (tmppath_t *tmppath);
void tmppath_release (tmppath_t *tmppath, char *path);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_TMPPATH_H */
