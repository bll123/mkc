/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mstime_t {
  struct timeval    tm;
} mstime_t;

void mssleep (uint32_t ms);
void mstimestart (mstime_t *mstm);
time_t mstimeend (mstime_t *mstm);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif
