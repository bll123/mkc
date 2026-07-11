/* this is a compilation test */
/* if the compilation fails, the type does not exist */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
${MKC_TV_TEST_HEADER_LIST}

int
main (void) {
#if defined (${MKC_TV_TEST_DEFINE})
  return 0;
#else
  generate a compilation error
#endif
}
