/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *  (from ballroomdj4)
 */

#ifndef MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#if __has_include (<tchar.h>)
# include <tchar.h>
#endif

#include "mkc_env.h"
#include "mkc_string.h"

void
mkc_env_get (const char *name, char *buff, size_t sz)
{
#if _function__wgetenv_s || (MKC_BOOTSTRAP && MKC_SYS_WIN)
  wchar_t     *wname;
  wchar_t     *wenv;
  char        *tenv;
  size_t      rv;

  wenv = malloc (sz);

  *buff = '\0';
  wname = mkc_towide (name);
  _wgetenv_s (&rv, wenv, sz, wname);
  free (wname);
  if (rv > 0) {
    tenv = mkc_fromwide (wenv);
    stpecpy (buff, buff + sz, tenv);
    free (tenv);
  }
  free (wenv);
#else
  char    *tptr;

  *buff = '\0';
  tptr = getenv (name);
  if (tptr != NULL) {
    stpecpy (buff, buff + sz, tptr);
  }
#endif
}

int
mkc_env_set (const char *name, const char *value)
{
  int     rc;

#if _function__wputenv_s || (MKC_BOOTSTRAP && MKC_SYS_WIN)
  {
    wchar_t *wname;
    wchar_t *wvalue;

    wname = mkc_towide (name);
    wvalue = mkc_towide (value);
    rc = _wputenv_s (wname, wvalue);
    free (wname);
    free (wvalue);
  }
#elif _function_setenv
  /* setenv is better */
  if (value != NULL && *value) {
    rc = setenv (name, value, 1);
  } else {
    rc = unsetenv (name);
  }
#else
  {
    char    tbuff [4096];
    snprintf (tbuff, sizeof (tbuff), "%s=%s", name, value);
    rc = putenv (tbuff);
  }
#endif
  return rc;
}

