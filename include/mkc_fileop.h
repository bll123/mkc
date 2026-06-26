/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_FILEOP_H
#define INC_MKC_FILEOP_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "mkc_error.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

MKC_NODISCARD FILE * mkc_fopen (const char *fname, const char *mode);
MKC_NODISCARD ssize_t mkc_file_size (const char *fname);
MKC_NODISCARD time_t mkc_file_modtime (const char *fname);
MKC_NODISCARD char * mkc_read_file (const char *fn, size_t *sz, mkc_error_t *mkcerr);
int mkc_file_delete (const char *fname);
int mkc_file_move (const char *fname, const char *nfn);
int64_t mkc_ftell (FILE *fh);
int mkc_fseek (FILE *fh, int64_t offset, int whence);
int mkc_file_copy (const char *fname, const char *nfn, mkc_error_t *mkcerr);
int mkc_link_copy (const char *fname, const char *nfn, mkc_error_t *mkcerr);
void mkc_display_path (char *path, size_t sz);
void mkc_normalize_path (char *path, size_t sz);

#if _function_symlink || (MKC_BOOTSTRAP && ! _WIN32)
int mkc_link_create (const char *target, const char *linkpath);
#endif /* _function_symlink*/

bool mkc_is_directory (const char *fname);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_FILEOP_H */
