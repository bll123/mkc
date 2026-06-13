/* this is a compilation test */
/* if the compilation fails, the constant does not exist */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
${MKC_TV_TEST_HEADER_LIST}

int
main (void)
{
  if (${MKC_TV_TEST_CONSTANT} == 0) {
    1;
  }
  return 0;
}
