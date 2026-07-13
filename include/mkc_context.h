/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_CONTEXT_H
#define INC_MKC_CONTEXT_H

#include "mkc_error.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  MKC_CONTEXT_ALTERNATE     = (1 << 0),
  MKC_CONTEXT_CACHE         = (1 << 1),
  MKC_CONTEXT_CHECK         = (1 << 2),
  MKC_CONTEXT_CHK_INC       = (1 << 3),
  MKC_CONTEXT_COMP_FLAG     = (1 << 4),
  MKC_CONTEXT_CONFIGURE     = (1 << 5),
  MKC_CONTEXT_GENERAL       = (1 << 6),
  MKC_CONTEXT_LOOP          = (1 << 7),
  MKC_CONTEXT_PROFILE       = (1 << 8),
  MKC_CONTEXT_PROJECT       = (1 << 9),
  MKC_CONTEXT_SET           = (1 << 10),
  MKC_CONTEXT_SUCCESS_FAIL  = (1 << 11),
} mkc_ctxt_val_t;

typedef struct mkc_context_t mkc_context_t;

MKC_NODISCARD mkc_context_t *mkc_context_init (mkc_error_t *mkcerr);
void mkc_context_free (mkc_context_t *context);
void mkc_context_push (mkc_context_t *context, mkc_ctxt_val_t ctxtval, mkc_error_t *mkcerr);
void mkc_context_pop (mkc_context_t *context);
bool mkc_context_check (mkc_context_t *context, mkc_ctxt_val_t ctxtval);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_CONTEXT_H */
