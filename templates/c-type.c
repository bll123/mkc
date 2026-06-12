/* this is a compilation test */
/* if the compilation fails, the type does not exist */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
${MKC_I_TEST_HEADER_LIST}

struct mkcstruct {
  ${MKC_I_TEST_TYPE} testmember;
};

static struct mkcstruct v;

struct mkcstruct * f() {
  return &v;
}

int
main (void) {
  struct mkcstruct *tmp;

  tmp = f();

  return 0;
}
