/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_UTIL_H
#define INC_MKC_UTIL_H

#include <stdbool.h>

#include "mkc_compiler.h"
#include "mkc_error.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

bool mkc_flag_is_libloc (mkc_compiler_id_t compid, const char *str);
void mkc_clean_mkcfiles (const char *project, char *tbuff, size_t tsz, mkc_error_t *mkcerr);
int mkc_create_mkcfiles_tmp (char *tbuff, size_t tsz, mkc_error_t *mkcerr);
int mkc_create_mkcfiles (char *tbuff, size_t tsz, mkc_error_t *mkcerr);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_UTIL_H */
