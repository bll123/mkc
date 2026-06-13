/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "mkc_context.h"
#include "mkc_error.h"

enum {
  MKC_CONTEXT_STACK_MAX = 20,
};

typedef struct mkc_context_t {
  mkc_ctxt_val_t  val [MKC_CONTEXT_STACK_MAX];
  int             stacksz;
  int             idx;
} mkc_context_t;

mkc_context_t *
mkc_context_init (mkc_error_t *mkcerr)
{
  mkc_context_t   *context;

  context = malloc (sizeof (mkc_context_t));
  if (context == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY);
    return NULL;
  }

  context->idx = 0;
  context->stacksz = 0;
  context->val [context->stacksz] = MKC_CONTEXT_GENERAL;
  context->stacksz += 1;

  return context;
}

void
mkc_context_free (mkc_context_t *context)
{
  if (context == NULL) {
    return;
  }

  free (context);
}

void
mkc_context_push (mkc_context_t *context, mkc_ctxt_val_t ctxtval,
    mkc_error_t *mkcerr)
{
  if (context == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_INVALID_ARGUMENT);
    return;
  }

  if (context->stacksz >= MKC_CONTEXT_STACK_MAX) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_RANGE);
    return;
  }

  context->val [context->stacksz] = ctxtval;
  context->stacksz += 1;
  context->idx += 1;
}

void
mkc_context_pop (mkc_context_t *context)
{
  if (context == NULL) {
    return;
  }

  if (context->stacksz > 1) {
    context->idx -= 1;
    context->stacksz -= 1;
  }

  return;
}

bool
mkc_context_check (mkc_context_t *context, mkc_ctxt_val_t ctxtval)
{
  if (context == NULL) {
    return false;
  }

  if (ctxtval == context->val [context->idx]) {
    return true;
  }

  return false;
}
