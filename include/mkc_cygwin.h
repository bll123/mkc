/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_CYGWIN_H
#define INC_MKC_CYGWIN_H

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <wchar.h>
#include <sys/types.h>

/* cygwin does not declare all windows functions and types */

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

/* envutil.c */
errno_t _wgetenv_s (size_t *_ReturnSize, wchar_t *_DstBuf,size_t _DstSizeInWords,const wchar_t *_VarName);
errno_t _wputenv_s (const wchar_t *_Name, const wchar_t *_Value);

/* fileop.c */
FILE * _wfopen (const wchar_t * _Filename, const wchar_t * _Mode);
int _wunlink (const wchar_t *_Filename);
int _wrename (const wchar_t *_OldFilename, const wchar_t *_NewFilename);

#  undef st_atime
#  undef st_mtime
#  undef st_ctime
  struct _stat64 {
        dev_t st_dev;
        ino_t st_ino;
        unsigned short st_mode;
        short st_nlink;
        short st_uid;
        short st_gid;
        dev_t st_rdev;
        int64_t st_size;
        int64_t st_atime;
        int64_t st_mtime;
        int64_t st_ctime;
  };

int _wstat64 (const wchar_t *_Filename, struct _stat64 *_Stat);
#  define __stat64 _stat64

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_CYGWIN_H */
