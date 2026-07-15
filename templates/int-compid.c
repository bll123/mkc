/* Copyright 2026 Brad Lanam Pleasant Hill CA */

/* this is a compile and execute test */

#include <stddef.h>

#include "mkc_compiler.h"

int
main (void)
{
#if defined (__GNUC__) && ! defined (__clang__)
  return MKC_COMP_ID_GCC;
#elif defined (__clang__)
  return MKC_COMP_ID_CLANG;
#elif defined (_MSC_VER) && ! defined (__INTEL_COMPILER) && ! defined (__clang__)
  return MKC_COMP_ID_MSC;
#elif defined (__ICC)
  return MKC_COMP_ID_ICC;
#elif defined (__ibmxl__)
  return MKC_COMP_ID_XLC;
#else
  return MKC_COMP_ID_UNKNOWN;
#endif
}
