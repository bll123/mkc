#include <stddef.h>

#include "mkc_def.h"

int
main (void)
{
#if defined (__MSYS_ID__)
  return MKC_SYS_ID_MSYS2;
#endif
#if defined (__CYGWIN__) && ! defined (__MSYS_ID__)
  return MKC_SYS_ID_CYGWIN;
#endif
#if defined (__FreeBSD__)
  return MKC_SYS_ID_FREEBSD;
#endif
#if defined (__NetBSD__)
  return MKC_SYS_ID_NETBSD;
#endif
#if defined (__OpenBSD__)
  return MKC_SYS_ID_OPENBSD;
#endif
#if defined (__DragonFly__)
  return MKC_SYS_ID_DRAGONFLYBSD;
#endif
#if defined (__SunOS)
  return MKC_SYS_ID_SOLARIS;
#endif
  return MKC_SYS_ID_NOTSET;
}
