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
#include "list.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"
#include "value.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

enum {
  MKC_VAR_FOUND = MKC_LIST_FOUND,
  MKC_VAR_NOTFOUND = MKC_LIST_NOTFOUND,
};

typedef struct var_t var_t;
typedef struct varlist_t     varlist_t;
typedef listidx_t mkc_varidx_t;

MKC_NODISCARD varlist_t     *varlist_init (mkc_log_t *log, mkc_error_t *mkcerr);
void varlist_free (varlist_t     *varlist);
void var_set_fromcache (varlist_t     *varlist, bool flag);

int var_set (varlist_t     *varlist, const char *vname, value_t *value);
void var_set_context (varlist_t     *varlist, const char *vname, int vctxt);

void var_delete (varlist_t     *varlist, const char *vname);

int32_t var_size (varlist_t     *varlist);
void var_iter_start (varlist_t     *varlist, mkc_varidx_t *iteridx);
mkc_varidx_t var_iter_next (varlist_t     *varlist, mkc_varidx_t *iteridx);
value_t *var_get_value (varlist_t     *varlist, const char *vname);
value_t *var_get_value_by_idx (varlist_t     *varlist, mkc_varidx_t vidx);
const char * var_get_name (varlist_t     *varlist, mkc_varidx_t idx);
bool var_is_defined (varlist_t     *varlist, const char *vname);
bool var_is_list (varlist_t     *varlist, const char *vname);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_VAR_H */
