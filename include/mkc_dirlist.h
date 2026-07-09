/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *    (from ballroomdj4)
 */
#ifndef INC_MKC_DIRLIST_H
#define INC_MKC_DIRLIST_H

#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

enum {
  DIRLIST_FILES = (1 << 0),
  DIRLIST_DIRS  = (1 << 1),
  /* If DIRLIST_LINKS is specified, links will be treated separately */
  /* and added to the file list.  Only for recursive. */
  DIRLIST_LINKS = (1 << 2),
};

MKC_NODISCARD mkc_list_t * mkc_dir_basic_list (const char *dirname, mkc_error_t *mkcerr);
MKC_NODISCARD mkc_list_t * mkc_dir_list_recursive (const char *dirname, int flags, mkc_error_t *mkcerr);

#if defined (__cplusplus) || defined (c_plusplus)
} /* extern C */
#endif

#endif /* INC_MKC_DIRLIST_H */
