/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_STRING_H
#define INC_MKC_STRING_H

#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

char * stpecpy (char *dst, char *end, const char *restrict src);
void mkc_strupper (char *buff);
char * mkc_strtok (char *buff, const char *delim, char **tokstr);
void mkc_strtrim (char *buff, size_t sz);

#if defined (_WIN32)

MKC_NODISCARD wchar_t * mkc_towide (const char *buff);
MKC_NODISCARD char * mkc_fromwide (const wchar_t *buff);

#endif /* _WIN32 */

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_STRING_H */
