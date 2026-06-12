#include <stddef.h>

#include "mkc_def.h"

int
main (void)
{
#if defined (__cplusplus)
  return MKC_COMP_CXX;
#elif defined (__OBJC__)
  return MKC_COMP_OBJC;
#else
  return MKC_COMP_C;
#endif
}
