/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "context.h"
#include "mkc_error.h"

enum {
  MKC_CONTEXT_STACK_MAX = 20,
};

typedef struct context_t {
  mkc_ctxt_val_t  val [MKC_CONTEXT_STACK_MAX];
  int             stacksz;
  int             idx;
} context_t;

MKC_NODISCARD
context_t *
context_init (mkc_error_t *mkcerr)
{
  context_t   *context;

  context = malloc (sizeof (context_t));
  if (context == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  context->idx = 0;
  context->stacksz = 0;
  context->val [context->stacksz] = MKC_CONTEXT_GENERAL;
  context->stacksz += 1;

  return context;
}

void
context_free (context_t *context)
{
  if (context == NULL) {
    return;
  }

  free (context);
}

void
context_push (context_t *context, mkc_ctxt_val_t ctxtval,
    mkc_error_t *mkcerr)
{
  if (context == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return;
  }

  if (context->stacksz >= MKC_CONTEXT_STACK_MAX) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_RANGE, 0, NULL);
    return;
  }

  context->val [context->stacksz] = ctxtval;
  context->stacksz += 1;
  context->idx += 1;
}

void
context_pop (context_t *context)
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
context_check (context_t *context, mkc_ctxt_val_t ctxtval)
{
  mkc_ctxt_val_t    ctxt;

  if (context == NULL) {
    return false;
  }

  /* ctxtval is a set of allowed values */
  /* ctxt is the current context */
  ctxt = context->val [context->idx];
  if ((ctxtval & ctxt) == ctxt) {
    return true;
  }

  return false;
}
