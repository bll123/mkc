/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "mkc_compiler.h"
#include "mkc_util.h"

bool
mkc_flag_is_libloc (mkc_compiler_id_t compid, const char *str)
{
  size_t    len;

  len =compiler_get_flag_len (compid, MKC_COMP_FLAG_LINKPREFIX);
  if (strncmp (str,
      compiler_get_flag (compid, MKC_COMP_FLAG_LINKPREFIX), len) == 0) {
    str += len;
  }

  if (strncmp (str,
      compiler_get_flag (compid, MKC_COMP_FLAG_LIBPATH),
      compiler_get_flag_len (compid, MKC_COMP_FLAG_LIBPATH)) == 0) {
    return true;
  }

  return false;
}

