/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *    (from ballroomdj4)
 */
#ifndef INC_MKC_DIROP_H
#define INC_MKC_DIROP_H

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

enum {
  DIROP_ALL             = 0,
  DIROP_ONLY_IF_EMPTY   = (1 << 0),
};

int   mkc_dirop_make (const char *dirname);
bool  mkc_dirop_delete (const char *dir, int flags);

#if defined (__cplusplus) || defined (c_plusplus)
} /* extern C */
#endif

#endif /* INC_MKC_DIROP_H */
