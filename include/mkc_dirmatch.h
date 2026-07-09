/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_DIRMATCH_H
#define INC_MKC_DIRMATCH_H

#if ! defined (MKC_BOOTSTRAP)
# include "mkc_config.h"
#endif

#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_nodiscard.h"
#include "mkc_regex.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

#if _have_regex
MKC_NODISCARD mkc_list_t * mkc_dir_match (const char *dirname, mkc_regex_t *rx, mkc_error_t *mkcerr);
#endif

#if defined (__cplusplus) || defined (c_plusplus)
} /* extern C */
#endif

#endif /* INC_MKC_DIRMATCH_H */
