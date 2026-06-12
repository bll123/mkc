#include <stddef.h>

#include "mkc_def.h"

int
main (void)
{
#if defined (__ICC)
  return MKC_COMP_ID_ICC;
#endif
#if defined (__ibmxl__)
  return MKC_COMP_ID_XLC;
#endif
#if defined (_MSC_VER) && ! defined (__INTEL_COMPILER) && ! defined (__clang__)
  return MKC_COMP_ID_MSC;
#endif
#if defined (__GNUC__) && ! defined (__clang__)
  return MKC_COMP_ID_GCC;
#endif
#if defined (__clang__)
  return MKC_COMP_ID_CLANG;
#endif
#if defined (__MINGW64__)
  return MKC_COMP_ID_MINGW;
#endif
  return MKC_COMP_ID_UNKNOWN;
}
