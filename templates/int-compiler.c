#include <stddef.h>

#include "mkc_def.h"

int
main (void)
{
#if defined (__cplusplus)
  return MKC_COMP_CXX;
#endif
#if defined (__OBJC__)
  return MKC_COMP_OBJC;
#endif
  return MKC_COMP_C;
}
