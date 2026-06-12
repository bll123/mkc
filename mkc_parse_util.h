/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#pragma once

#include <stdint.h>

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

char *mkc_parse_set_str (const char *txt);
void mkc_parse_free_str (char *str);
void mkc_parse_msg (const char *msg, int32_t lineno, int colno);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif
