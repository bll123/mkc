/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_OPTION_H
#define INC_MKC_OPTION_H

#include <stdbool.h>
#include <stdint.h>

#include "mkc_log.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mkc_option_t {
  char          * currprofile;
  const char    * mkc_filename;
  const char    * stage;
  const char    * prefix;
  int32_t       loglevel;
  log_verbose_t verbose;
  bool          clean;
  bool          retest;
} mkc_option_t;

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_OPTION_H */
