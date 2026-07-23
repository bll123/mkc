/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_ENV_H
#define INC_MKC_ENV_H

#include <stddef.h>

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

void env_get (const char *name, char *buff, size_t sz);
int env_set (const char *name, const char *value);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_ENV_H */
