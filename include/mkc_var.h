/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_VAR_H
#define INC_MKC_VAR_H

#include <stdint.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  MKC_VT_INVALID,
  /* basic types */
  /* these are present in the variable list */
  MKC_VT_INTEGER,
  MKC_VT_LIST,
  MKC_VT_RANGE,
  MKC_VT_STRING,
  MKC_VT_TIMESTAMP,
  /* used by the parser and processor */
  MKC_VT_STATIC_STRING,
  MKC_VT_QUOTED_STRING,
  MKC_VT_VARIABLE,
  MKC_VT_ENV_VARIABLE,
} mkc_var_type_t;

/* variable context, used to determine if the variable should be output */
typedef enum {
  MKC_VCTXT_CHECK,
  MKC_VCTXT_DELETED,      /* not yet implemented */
  MKC_VCTXT_ENV,
  MKC_VCTXT_FLAG,
  MKC_VCTXT_MKC,
  MKC_VCTXT_TEMP,
  MKC_VCTXT_USER_DISABLE,
  MKC_VCTXT_USER_ENABLE,
} mkc_var_ctxt_t;

enum {
  MKC_VAR_FOUND = MKC_LIST_FOUND,
  MKC_VAR_NOTFOUND = MKC_LIST_NOTFOUND,
};

typedef struct mkc_var_t mkc_var_t;
typedef struct mkc_varlist_t mkc_varlist_t;
typedef mkc_listidx_t mkc_varidx_t;

typedef struct mkc_range_t {
  int32_t     beg;
  int32_t     end;
  int32_t     incr;
  int32_t     var;
  bool        finish;
} mkc_range_t;

typedef struct mkc_value_t {
  union {
    mkc_list_t  *list;
    char        *sval;
    mkc_range_t range;
    int32_t     ival;
    time_t      tmval;
  };
  mkc_var_type_t  vtype;
  mkc_var_ctxt_t  vctxt;
  bool            tempallocated;
} mkc_value_t;

MKC_NODISCARD mkc_varlist_t *mkc_varlist_init (mkc_log_t *log, mkc_error_t *mkcerr);
void mkc_varlist_free (mkc_varlist_t *varlist);
void mkc_var_set_fromcache (mkc_varlist_t *varlist, bool flag);
const char *mkc_var_vctxt_str (mkc_var_ctxt_t vctxt);

int mkc_var_set (mkc_varlist_t *varlist, const char *vname, mkc_value_t *value);
void mkc_var_set_context (mkc_varlist_t *varlist, const char *vname, int vctxt);

int32_t mkc_var_size (mkc_varlist_t *varlist);
void mkc_var_iter_start (mkc_varlist_t *varlist, mkc_varidx_t *iteridx);
mkc_varidx_t mkc_var_iter_next (mkc_varlist_t *varlist, mkc_varidx_t *iteridx);
mkc_value_t *mkc_var_get_value (mkc_varlist_t *varlist, const char *vname);
mkc_value_t *mkc_var_get_value_by_idx (mkc_varlist_t *varlist, mkc_varidx_t vidx);
const char * mkc_var_get_name (mkc_varlist_t *varlist, mkc_varidx_t idx);
bool mkc_var_is_defined (mkc_varlist_t *varlist, const char *vname);
bool mkc_var_is_list (mkc_varlist_t *varlist, const char *vname);

void mkc_value_init (mkc_value_t *value);
void mkc_value_free (void *value);
const char *mkc_value_to_str (mkc_value_t *value, char *buff, size_t sz);
void mkc_value_range_init (mkc_value_t *value, int32_t beg, int32_t end, int32_t incr);
void mkc_value_range_iter_start (mkc_value_t *value, mkc_listidx_t *iteridx);
int mkc_value_range_iter_next (mkc_value_t *value, mkc_value_t *rval, mkc_listidx_t *iteridx);

void mkc_value_iter_start (mkc_value_t *value, mkc_listidx_t *iteridx);
int mkc_value_iter_next (mkc_value_t *value, mkc_value_t *rval, mkc_listidx_t *iteridx);

const char * mkc_value_disp_type (mkc_value_t *value);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_VAR_H */
