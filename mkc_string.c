/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#if ! MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if __has_include (<windows.h>)
# define WIN32_LEAN_AND_MEAN 1
# include <windows.h>
#endif

#include "mkc_string.h"

#if ! _lib_stpecpy

# define STPECPY_DEBUG 0

/* the following code is in the public domain */
/* modified from the linux stpecpy manual page */

char *
stpecpy (char *dst, char *end, const char *restrict src)
{
  char  *p;

#if defined STEPCPY_DEBUG
  if (end - dst == sizeof (char *)) {
    fprintf (stderr, "WARN: stpecpy: length set to sizeof (char *)\n");
  }
#endif
#if defined (STPECPY_DEBUG)
  if (dst == NULL) {
    fprintf (stderr, "ERR: stpecpy: null destination\n");
  }
  if (end == NULL) {
    fprintf (stderr, "ERR: stpecpy: null end pointer\n");
  }
  if (src == NULL) {
    fprintf (stderr, "ERR: stpecpy: null source\n");
  }
#endif

  if (src == NULL || dst == NULL || end == NULL) {
    return end;
  }
  if (dst == end) {
    return end;
  }

  p = memccpy (dst, src, '\0', end - dst);
  if (p != NULL) {
    return p - 1;
  }

  /* truncation detected */
  end [-1] = '\0';
  return end;
}

#endif /* ! _lib_stpecpy */

void
mkc_strupper (char *buff)
{
  size_t    len;

  len = strlen (buff);
  for (size_t i = 0; i < len; ++i) {
    buff [i] = toupper (buff [i]);
  }
}

char *
mkc_strtok (char *buff, const char *delim, char **tokstr)
{
  char    *tmp;

#if _function_strtok_r
  tmp = strtok_r (buff, delim, tokstr);
#else
  tmp = strtok (buff, delim);
#endif

  return tmp;
}

void
mkc_strtrim (char *buff, size_t sz)
{
  char    *p;

  if (sz == 0) {
    sz = strlen (buff);
  }
  p = buff + sz - 1;
  while (p >= buff && (*p == ' ' || *p == '\n' || *p == '\r')) {
    *p = '\0';
    --p;
  }
}

void
mkc_strclean (char *buff, size_t sz)
{
  if (sz == 0) {
    sz = strlen (buff);
  }
  for (size_t i = 0; i < sz; ++i) {
    if (! isalnum (buff [i])) {
      buff [i] = '_';
    }
  }
}

void
datafree (void *data)
{
  if (data == NULL) {
    return;
  }
  free (data);
}

#if defined (_WIN32)

MKC_NODISCARD
wchar_t *
mkc_towide (const char *buff)
{
  wchar_t     *tbuff = NULL;
  size_t      len;

  /* len does not include room for the null byte */
  len = MultiByteToWideChar (CP_UTF8, 0, buff, strlen (buff), NULL, 0);
  tbuff = malloc ((len + 1) * sizeof (wchar_t));
  if (tbuff != NULL) {
    MultiByteToWideChar (CP_UTF8, 0, buff, strlen (buff), tbuff, len);
    tbuff [len] = L'\0';
  }
  return tbuff;
}

MKC_NODISCARD
char *
mkc_fromwide (const wchar_t *buff)
{
  char        *tbuff = NULL;
  size_t      len;

  /* len does not include room for the null byte */
  len = WideCharToMultiByte (CP_UTF8, 0, buff, -1, NULL, 0, NULL, NULL);
  tbuff = malloc (len + 1);
  if (tbuff != NULL) {
    WideCharToMultiByte (CP_UTF8, 0, buff, -1, tbuff, len, NULL, NULL);
    tbuff [len] = '\0';
  }
  return tbuff;
}

#endif /* _WIN32 */

