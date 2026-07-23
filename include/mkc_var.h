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
#include "value.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

enum {
  MKC_VAR_FOUND = MKC_LIST_FOUND,
  MKC_VAR_NOTFOUND = MKC_LIST_NOTFOUND,
};

typedef struct mkc_var_t mkc_var_t;
typedef struct mkc_varlist_t mkc_varlist_t;
typedef mkc_listidx_t mkc_varidx_t;

MKC_NODISCARD mkc_varlist_t *mkc_varlist_init (mkc_log_t *log, mkc_error_t *mkcerr);
void mkc_varlist_free (mkc_varlist_t *varlist);
void mkc_var_set_fromcache (mkc_varlist_t *varlist, bool flag);

int mkc_var_set (mkc_varlist_t *varlist, const char *vname, value_t *value);
void mkc_var_set_context (mkc_varlist_t *varlist, const char *vname, int vctxt);

int32_t mkc_var_size (mkc_varlist_t *varlist);
void mkc_var_iter_start (mkc_varlist_t *varlist, mkc_varidx_t *iteridx);
mkc_varidx_t mkc_var_iter_next (mkc_varlist_t *varlist, mkc_varidx_t *iteridx);
value_t *mkc_var_get_value (mkc_varlist_t *varlist, const char *vname);
value_t *mkc_var_get_value_by_idx (mkc_varlist_t *varlist, mkc_varidx_t vidx);
const char * mkc_var_get_name (mkc_varlist_t *varlist, mkc_varidx_t idx);
bool mkc_var_is_defined (mkc_varlist_t *varlist, const char *vname);
bool mkc_var_is_list (mkc_varlist_t *varlist, const char *vname);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_VAR_H */
