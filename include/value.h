/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_VALUE_H
#define INC_VALUE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "mkc_list.h"

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
} value_type_t;

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
} value_ctxt_t;

typedef struct range_t {
  int32_t     beg;
  int32_t     end;
  int32_t     incr;
  int32_t     var;
  bool        finish;
} range_t;

typedef struct value_t {
  union {
    mkc_list_t  * list;
    char        * sval;
    range_t     range;
    int32_t     ival;
    time_t      tmval;
  };
  value_type_t    vtype;
  value_ctxt_t    vctxt;
  bool            tempallocated;
} value_t;

void value_init (value_t *value);
void value_free (void *value);
const char *value_to_str (value_t *value, char *buff, size_t sz);
void value_range_init (value_t *value, int32_t beg, int32_t end, int32_t incr);
void value_range_iter_start (value_t *value, mkc_listidx_t *iteridx);
int value_range_iter_next (value_t *value, value_t *rval, mkc_listidx_t *iteridx);

void value_iter_start (value_t *value, mkc_listidx_t *iteridx);
int value_iter_next (value_t *value, value_t *rval, mkc_listidx_t *iteridx);

bool value_is_string_type (const value_t *value);

const char * value_disp_type (value_t *value);
const char * value_ctxt_str (value_ctxt_t vctxt);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_VALUE_H */
