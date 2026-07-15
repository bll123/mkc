/* Copyright 2026 Brad Lanam Pleasant Hill CA */

/* this is a compilation test */
/* if the compilation fails, the header does not exist, */
/* or does not compile */

${MKC_TV_TEST_HEADER_LIST}

/* the calling process will put in quotes or angle brackets */

#include ${MKC_TV_TEST_HEADER}

int
main (void)
{
  return 0;
}
