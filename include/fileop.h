/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_FILEOP_H
#define INC_FILEOP_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "mkc_error.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

MKC_NODISCARD FILE * fileop_open (const char *fname, const char *mode);
bool fileop_exists (const char *fname);
time_t fileop_modtime (const char *fname);
ssize_t fileop_size (const char *fname);
bool fileop_is_link (const char *fname);

MKC_NODISCARD char * fileop_read_file (const char *fn, size_t *sz, mkc_error_t *mkcerr);
int fileop_file_delete (const char *fname);
int fileop_file_move (const char *fname, const char *nfn);
int64_t fileop_tell (FILE *fh);
int fileop_seek (FILE *fh, int64_t offset, int whence);
int fileop_file_copy (const char *fname, const char *nfn, mkc_error_t *mkcerr);
int fileop_link_copy (const char *fname, const char *nfn, mkc_error_t *mkcerr);
void fileop_display_path (char *path, size_t sz);
void fileop_normalize_path (char *path, size_t sz);

#if _function_symlink || (MKC_BOOTSTRAP && ! MKC_SYS_WIN)
int fileop_link_create (const char *target, const char *linkpath);
#endif /* _function_symlink*/

bool fileop_is_directory (const char *fname);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_FILEOP_H */
