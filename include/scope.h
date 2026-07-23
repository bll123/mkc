/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_SCOPE_H
#define INC_SCOPE_H

#include <stdint.h>
#include <stdbool.h>

#include "mkc_compiler.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_var.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  SCOPE_T_INTERNAL,
  SCOPE_T_CURR_PROF,
  /* the current-profile-compiler type encompasses all compilers */
  SCOPE_T_CURR_PROF_COMPILER,
  SCOPE_T_LOCAL,
  SCOPE_T_TARGET,
  SCOPE_T_NOT_IN_USE,
  /* use in-scope to get the variable using the hierarchy */
  SCOPE_T_IN_SCOPE,
  /* temporary, timestamps, dependencies and paths have their own namespace */
  SCOPE_T_TIMESTAMP,
  SCOPE_T_DEPENDENCIES,
  SCOPE_T_PATHS,
} scope_type_t;

typedef enum {
  SCOPE_NO_ESCAPE,
  SCOPE_SUB_ESCAPE,
} scope_escape_t;

typedef struct scope_t scope_t;

scope_t * scope_init (mkc_log_t *log, mkc_error_t *mkcerr);
void scope_free (scope_t *scope);
void scope_push (scope_t *scope, scope_type_t sctype);
void scope_pop (scope_t *scope);
void scope_set_curr_compiler (scope_t *scope, mkc_compiler_t compiler);
void scope_set_fromcache (scope_t *scope, bool flag);

time_t scope_get_timestamp (scope_t *scope, const char *vname);
value_t * scope_get_value (scope_t *scope, scope_type_t sctype, const char *vname);
int32_t scope_value_get_integer (scope_t *scope, value_t *value);
time_t scope_value_get_timestamp (scope_t *scope, value_t *value);
void scope_value_get_str (scope_t *scope, value_t *value, char *buff, size_t sz);
value_t * scope_value_get_value (scope_t *scope, value_t *value);
value_t * scope_value_get_list_value (scope_t *scope, value_t *value);
int scope_append_str_list (scope_t *scope, scope_type_t sctype, const char *vname, const char *data, value_ctxt_t vctxt);

void scope_set_context (scope_t *scope, const char *vname, value_ctxt_t vctxt);
int scope_set (scope_t *scope, scope_type_t sctype, const char *vname, value_t *value, value_ctxt_t vctxt);
int scope_set_integer (scope_t *scope, scope_type_t sctype, const char *vname, int32_t ival, value_ctxt_t vctxt);
int scope_set_timestamp (scope_t *scope, scope_type_t sctype, const char *vname, time_t tmval, value_ctxt_t vctxt);
int scope_set_str (scope_t *scope, scope_type_t sctype, const char *vname, const char *str, value_ctxt_t vctxt);
int scope_set_list (scope_t *scope, scope_type_t sctype, const char *vname, mkc_list_t *list, value_ctxt_t vctxt);
int scope_set_list_from_str (scope_t *scope, const char *vname, char *str, value_ctxt_t vctxt);

bool scope_is_defined (scope_t *scope, const char *vname);
bool scope_var_is_list (scope_t *scope, const char *vname);
void scope_temp_value_free (void *tvalue);
char * scope_substitute (scope_t *scope, const char *data, scope_escape_t subescapeflag, int depth);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_SCOPE_H */
