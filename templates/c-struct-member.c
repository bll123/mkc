/* this is a compilation test */
/* if the compilation fails, the structure/member does not exist */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
${MKC_TV_TEST_HEADER_LIST}

int
main (void)
{
  ${MKC_TV_TEST_STRUCT_NAME} a;
  size_t  b;

  b = sizeof (a.${MKC_TV_TEST_STRUCT_MEMBER});
  return 0;
}
