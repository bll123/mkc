/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_STRING_H
#define INC_MKC_STRING_H

#ifndef MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

char * stpecpy (char *dst, char *end, const char *restrict src);
void mkc_strupper (char *buff);
char * mkc_strtok (char *buff, const char *delim, char **tokstr);
void mkc_strtrim (char *buff, size_t sz);
void mkc_trim_char (char *buff, unsigned char c);
void mkc_strclean (char *buff, size_t sz);
void datafree (void *data);

#if defined (MKC_SYS_WIN)

#if _function_MultiByteToWideChar || (MKC_BOOTSTRAP && MKC_SYS_WIN)
MKC_NODISCARD wchar_t * mkc_towide (const char *buff);
#endif
#if _function_WideCharToMultiByte || (MKC_BOOTSTRAP && MKC_SYS_WIN)
MKC_NODISCARD char * mkc_fromwide (const wchar_t *buff);
#endif

#endif /* MKC_SYS_WIN */

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_STRING_H */
