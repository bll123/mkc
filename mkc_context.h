/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "mkc_def.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  MKC_CONTEXT_CHECK,
  MKC_CONTEXT_GENERAL,
  MKC_CONTEXT_LOOP,
  MKC_CONTEXT_MAX,
} mkc_ctxt_val_t;

typedef struct mkc_context_t mkc_context_t;

mkc_context_t *mkc_context_init (mkc_error_t *mkcerr);
void mkc_context_free (mkc_context_t *context);
void mkc_context_push (mkc_context_t *context, mkc_ctxt_val_t ctxtval, mkc_error_t *mkcerr);
void mkc_context_pop (mkc_context_t *context);
bool mkc_context_check (mkc_context_t *context, mkc_ctxt_val_t ctxtval);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif
