/* Copyright 2026 Brad Lanam Pleasant Hill CA */

/* this is a compile and execute test */

#include <stdio.h>
#include <stddef.h>

#include "mkc_compiler.h"

int
main (void)
{
  int     compid = MKC_COMP_ID_UNKNOWN;
#if defined (__GNUC__) && ! defined (__clang__)
  compid = MKC_COMP_ID_GCC;
#elif defined (__clang__)
  compid = MKC_COMP_ID_CLANG;
#elif defined (_MSC_VER) && ! defined (__INTEL_COMPILER) && ! defined (__clang__)
  compid = MKC_COMP_ID_MSC;
#elif defined (__ICC)
  compid = MKC_COMP_ID_ICC;
#elif defined (__ibmxl__)
  compid = MKC_COMP_ID_XLC;
#else
  compid = MKC_COMP_ID_UNKNOWN;
#endif

  fprintf (stdout, "%d\n", compid);
  return 0;
}
