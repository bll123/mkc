/* Copyright 2026 Brad Lanam Pleasant Hill CA */

/* this is a compile and execute test */

#include <stddef.h>
#include <stdlib.h>

#include "mkc_def.h"

#if defined (__APPLE__) && defined (__MACH__)
# include <TargetConditionals.h>
#endif
#if defined (__unix__) || ! defined (__APPLE__) && defined (__MACH__)
# include <sys/param.h>
#endif

int
main (void)
{
#if defined (_AIX)
  return MKC_SYS_AIX;
#endif
#if defined (__ANDROID__)
  return MKC_SYS_ANDROID;
#endif
#if defined (__linux__) && ! defined (__ANDROID__)
  return MKC_SYS_LINUX;
#endif
#if defined (_WIN32) || defined (__CYGWIN__)
  return MKC_SYS_WINDOWS;
#endif
#if defined (__APPLE__) && defined (__MACH__)
# if TARGET_IPHONE_SIMULATOR == 1
   return MKC_SYS_IOS;
# elif TARGET_OS_IPHONE == 1
   return MKC_SYS_IOS;
# elif TARGET_OS_MAC == 1
   return MKC_SYS_MACOS;
# endif
#endif
#if defined (BSD) || \
    defined (__FreeBSD__) || \
    defined (__NetBSD__) || \
    defined (__OpenBSD__) || \
    defined (__DragonFly__)
  return MKC_SYS_BSD;
#endif
  return MKC_SYS_UNKNOWN;
}
