/* Copyright 2026 Brad Lanam Pleasant Hill CA */

/* this is a compile and link test */
/* if the compile or link fails, the function does not exist */

/* include the headers that declare most of the standard functions */

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
#if defined (${MKC_TV_TEST_FUNCTION_NAME})
  return 0;
#else
  void    *f;

  f = &(${MKC_TV_TEST_FUNCTION_NAME});
  return 0;
#endif
}
