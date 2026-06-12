#include <stddef.h>
#include <sys/stat.h>

#include "mkc_def.h"

int
main (void)
{
#if defined (__linux__)
  int           rc;
  struct stat   statbuf;

  rc = stat ("/lib64/libc.so", &statbuf);
  if (rc == 0) {
    return MKC_LIB_LOC_LIB64;
  }
  rc = stat ("/lib64/libc.so.6", &statbuf);
  if (rc == 0) {
    return MKC_LIB_LOC_LIB64;
  }
  rc = stat ("/lib64/libc.so.7", &statbuf);
  if (rc == 0) {
    return MKC_LIB_LOC_LIB64;
  }

  return MKC_LIB_LOC_LIB;
#endif
  return MKC_LIB_LOC_NOTSET;
}
