/* this is a c-preprocessor test */
/* the c-preprocessor output is processed to get the function arguments */

/* clean up the argument list, remove all the extra stuff */
#if ${MKC_I_VARIADIC_MACRO}
# define __asm__(...)
# define __nonnull__(...)
# define __nonnull(...)
#else
# define __asm__(a)
# define __nonnull__(a)
# define __nonnull(a)
#endif
#define _Nonnull(a)
#define __asm(a)
#define __attribute__(a)
#define __restrict
#define __restrict__
#if defined(__THROW)
# undef __THROW
#endif
#define __THROW
#define __const const
#define extern
#define __wur
#define __extension__

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
${MKC_TV_TEST_HEADER_LIST}

int
main (void)
{
  return 0;
}
