/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_CHARARR_H
#define INC_CHARARR_H

#include <stddef.h>
#include <stdbool.h>

#include "mkc_error.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct chararr_t chararr_t;

chararr_t * chararr_init (mkc_error_t *mkcerr);
void chararr_free (chararr_t *carr);
int chararr_size (chararr_t *carr);
void chararr_reset (chararr_t *carr, int idx);
void chararr_append (chararr_t *carr, const char *txt);
void chararr_freeinternals (chararr_t *carr);
const char ** chararr_get_arr (chararr_t *carr);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_CHARARR_H */
