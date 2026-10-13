#include <stdio.h>
#include <stdlib.h>

int
main (void)
{
#if HW_HELLO
  printf ("hello ");
#endif
#if HW_WORLD
  printf ("world\n");
#endif
  return 0;
}
