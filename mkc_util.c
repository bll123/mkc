/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "mkc_util.h"

bool
mkc_flag_is_libloc (const char *str)
{
  if (strncmp (str, "-L", 2) == 0 ||
      strncmp (str, "-Wl,-L", 6) == 0) {
    return true;
  }

  return false;
}

