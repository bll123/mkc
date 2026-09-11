/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_DICTDICT_H
#define INC_DICTDICT_H

#include <stddef.h>
#include <stdint.h>

#include "dict.h"
#include "list.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"
#include "value.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

void dictdict_set (dict_t *dd, const char *name, const char *tag, value_t *data, mkc_log_t *log, list_free_t freefunc, mkc_error_t *mkcerr);
value_t * dictdict_get (dict_t *dd, const char *name, const char * tag, mkc_error_t *mkcerr);
void dictdict_delete (dict_t *dd, const char *name, const char * tag, mkc_error_t *mkcerr);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_DICTDICT_H */
