/* Copyright 2026 Brad Lanam Pleasant Hill CA */

/* this is a compile-only test */

#include <stddef.h>

int
main (void)
{
#if defined (__has_include)
# if __has_include (<stdio.h>)
  return 0;
# else
#  error "defined, but fail"
# endif
#else
# error "not defined"
#endif
}
