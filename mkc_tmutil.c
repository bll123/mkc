/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#ifndef MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#if __has_include (<windows.h>)
# define WIN32_LEAN_AND_MEAN 1
# include <windows.h>
#endif

#include "mkc_tmutil.h"

void
mssleep (uint32_t ms)
{
/* windows seems to have a very large amount of overhead when calling */
/* nanosleep() or Sleep(). */
/* macos seems to have a minor amount of overhead when calling nanosleep() */

/* on windows, nanosleep is within the libwinpthread msys2 library which */
/* is not wanted. So use the Windows API Sleep() function */
#if _function_Sleep || (MKC_BOOTSTRAP && MKC_SYS_WIN)
  Sleep ((DWORD) ms);
#endif
#if (! _function_Sleep && _function_nanosleep) || (MKC_BOOTSTRAP && ! MKC_SYS_WIN)
  struct timespec   ts;
  struct timespec   rem;

  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms - (ts.tv_sec * 1000)) * 1000 * 1000;
  while (ts.tv_sec > 0 || ts.tv_nsec > 0) {
    nanosleep (&ts, &rem);
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    /* remainder is only valid when EINTR is returned */
    /* most of the time, an interrupt is caused by a control-c while testing */
    /* just let an interrupt stop the sleep */
  }
#endif
}

void
mstimestart (mstime_t *mstm)
{
  gettimeofday (&mstm->tm, NULL);
}

time_t
mstimeend (mstime_t *mstm)
{
  struct timeval    end;
  time_t            s, u, m;

  gettimeofday (&end, NULL);
  s = end.tv_sec - mstm->tm.tv_sec;
  u = end.tv_usec - mstm->tm.tv_usec;
  m = s * 1000 + u / 1000;

  return m;
}

