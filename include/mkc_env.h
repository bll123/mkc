/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#pragma once

#include <stddef.h>

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

void mkc_env_get (const char *name, char *buff, size_t sz);
int mkc_env_set (const char *name, const char *value);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif
