/* this is a compile and execute test */
/* the return code from the execution indicates the size of the type */
/* if the compilation fails, the type does not exist */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
${MKC_TV_TEST_HEADER_LIST}

int
main (void)
{
  size_t    sz;

  sz = sizeof (${MKC_TV_TEST_SIZE});
  return sz;
}
