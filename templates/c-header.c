/* this is a compilation test */
/* if the compilation fails, the header does not exist */
${MKC_I_TEST_HEADER_LIST}
#include ${MKC_I_TEST_HEADER}

int
main (void)
{
  return 0;
}
