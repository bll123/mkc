/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_TMUTIL_H
#define INC_TMUTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct msint64_t {
  struct timeval    tm;
} msint64_t;

void mssleep (uint32_t ms);
int64_t mstime (void);
void mstimestart (msint64_t *mstm);
int64_t mstimeend (msint64_t *mstm);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_TMUTIL_H */
