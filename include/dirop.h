/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *    (from ballroomdj4)
 */
#ifndef INC_DIROP_H
#define INC_DIROP_H

#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

enum {
  DIROP_ALL             = 0,
  DIROP_ONLY_IF_EMPTY   = (1 << 0),
};

enum {
  DIRLIST_FILES = (1 << 0),
  DIRLIST_DIRS  = (1 << 1),
  /* If DIRLIST_LINKS is specified, links will be treated separately */
  /* and added to the file list.  Only for recursive. */
  DIRLIST_LINKS = (1 << 2),
};

typedef struct mkc_dirhandle_t mkc_dirhandle_t;

MKC_NODISCARD mkc_dirhandle_t * dirop_open (const char *dirname, mkc_error_t *mkcerr);
MKC_NODISCARD char * dirop_iterate (mkc_dirhandle_t *dirh, mkc_error_t *mkcerr);
void dirop_close (mkc_dirhandle_t *dirh);
int dirop_make (const char *dirname, mkc_error_t *mkcerr);
int dirop_delete (const char *dir, int flags, mkc_error_t *mkcerr);
MKC_NODISCARD mkc_list_t * dirop_basic_list (const char *dirname, mkc_error_t *mkcerr);
MKC_NODISCARD mkc_list_t * dirop_list_recursive (const char *dirname, int flags, mkc_error_t *mkcerr);

#if defined (__cplusplus) || defined (c_plusplus)
} /* extern C */
#endif

#endif /* INC_DIROP_H */
