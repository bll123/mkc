/* Copyright 2026 Brad Lanam Pleasant Hill CA */

#ifndef INC_B_H
#define INC_B_H

#include <stdio.h>

/* the test defines MKCTEST=1 */
/* the compile will fail if the c-flag is not properly set */
#ifndef MKCTEST
# error "no mkctest defined"
#endif

#endif /* INC_B_H */
