/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_SCOPE_H
#define INC_SCOPE_H

#include <stdint.h>
#include <stdbool.h>

#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_var.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  SCOPE_T_CURR_PROF,
  SCOPE_T_CURR_PROF_COMPILER,
  SCOPE_T_INTERNAL,
  SCOPE_T_LOCAL,
  SCOPE_T_NOT_IN_USE,
  SCOPE_T_TARGET,
} scope_type_t;

typedef struct scope_t scope_t;

scope_t * scope_init (mkc_log_t *log, mkc_error_t *mkcerr);
void scope_free (scope_t *scope);
void scope_push (scope_t *scope, scope_type_t sctype);
void scope_pop (scope_t *scope);
void scope_iter_start (scope_t *scope, int *iteridx);
mkc_varlist_t *scope_iter_next (scope_t *scope, int *iteridx);
mkc_varlist_t *scope_get (scope_t *scope, scope_type_t sctype);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_SCOPE_H */
