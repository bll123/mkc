#include <stddef.h>

int
main (void)
{
#if defined (__has_include)
# if __has_include (<stdio.h>)
  return 0;
# else
  return 1;
# endif
#else
  return 1;
#endif
}
