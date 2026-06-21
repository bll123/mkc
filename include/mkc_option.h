/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_OPTION_H
#define INC_MKC_OPTION_H

#include <stdbool.h>

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mkc_option_t {
  const char  * dfltprof;
  const char  * compilertxt;
  const char  * mkcfile_dir;
  const char  * stage;
  const char  * prefix;
  bool        retest;
} mkc_option_t;

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_OPTION_H */
