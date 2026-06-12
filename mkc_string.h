/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#pragma once

#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

char * stpecpy (char *dst, char *end, const char *restrict src);

#if defined (_WIN32)

MKC_NODISCARD wchar_t * mkc_towide (const char *buff);
MKC_NODISCARD char * mkc_fromwide (const wchar_t *buff);

#endif /* _WIN32 */

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif
