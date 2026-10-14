/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_PARSEFN_H
#define INC_PARSEFN_H

#include <stdio.h>
#include <stdint.h>

#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct parsefn_t parsefn_t;

/* parsefn.c */
MKC_NODISCARD parsefn_t * parsefn_init (mkc_log_t *log, mkc_error_t *mkcerr);
void parsefn_free (parsefn_t *parsefn);
void parsefn_set (parsefn_t *parsefn, const char *fname);
const char * parsefn_get (parsefn_t *parsefn);
void parsefn_pop (parsefn_t *parsefn);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_PARSEFN_H */
