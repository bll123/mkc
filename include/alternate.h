/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_ALTERNATE_H
#define INC_ALTERNATE_H

#include <stddef.h>
#include <stdbool.h>

#include "list.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

/* each alternate will have a name associated with it */
/* the first alternate in the list is the base context */
/* and stores the name for the check */
typedef struct mkc_alternate_t {
  char            * name;
  list_t      * hdrlist;
  list_t      * compflags;
  list_t      * linkflags;
  list_t      * libs;
} mkc_alternate_t;

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_ALTERNATE_H */
