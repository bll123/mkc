/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "mkc_parse_util.h"

char *
mkc_parse_set_str (const char *txt)
{
  char  *sval;

  sval = strdup (txt);
  return sval;
}

void
mkc_parse_free_str (char *str)
{
  if (str == NULL) {
    return;
  }
  free (str);
}

void
mkc_parse_msg (const char *msg, int32_t lineno, int colno)
{
  fprintf (stderr, "[%" PRId32 ":%d] %s\n", lineno, colno, msg);
}
