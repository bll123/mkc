/* Copyright 2026 Brad Lanam Pleasant Hill CA */

/* this is a compile and execute test */
/* the size of the type is printed to stdout */
/* if the compilation fails, the type does not exist */

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
  size_t    sz;

  sz = sizeof (${MKC_TV_TEST_SIZE});
  fprintf (stdout, "%zd\n", sz);
  return 0;
}
