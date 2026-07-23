/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_UTIL_H
#define INC_MKC_UTIL_H

#include <stdbool.h>

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

bool mkc_flag_is_libloc (const char *str);
void mkc_flags_free (char **flags);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_UTIL_H */
