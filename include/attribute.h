/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_ATTRIBUTE_H
#define INC_ATTRIBUTE_H

#include <stddef.h>
#include <stdbool.h>

#include "alternate.h"
#include "mkc_compiler.h"
#include "mkc_list.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  MKC_ATTR_INPUT,
  MKC_ATTR_LIB_VERSION,
  MKC_ATTR_MATCH,
  MKC_ATTR_METHOD,
  MKC_ATTR_NAMESPACE,
  MKC_ATTR_OUTPUT,
  MKC_ATTR_VCONTEXT,
  MKC_ATTR_VERSION,
  MKC_ATTR_MAX,
} mkc_attr_type_t;

typedef struct mkc_attribute_t {
  mkc_list_t      * alternates;
  mkc_alternate_t * curralt;
  char            * currname;
  char            * str [MKC_ATTR_MAX];
  mkc_list_t      * pathlist;
  mkc_list_t      * replacelist;
  mkc_list_t      * sourcelist;
  mkc_compiler_t  currcompiler;
  int             define_zero;
  int             headertype;
  bool            localheader;
  bool            negate;
  bool            printerrors;
} mkc_attribute_t;

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_ATTRIBUTE_H */
