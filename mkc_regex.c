/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *    (from ballroomdj4)
 */

#ifndef MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#if _package_glib

#include <glib.h>
#include <glib/gregex.h>

#include "mkc_error.h"
#include "mkc_regex.h"
#include "mkc_nodiscard.h"

typedef struct mkc_regex_t {
  GRegex      *regex;
  mkc_error_t *mkcerr;
} mkc_regex_t;

MKC_NODISCARD
mkc_regex_t *
mkc_regex_init (const char *pattern, mkc_error_t *mkcerr)
{
  mkc_regex_t  *rx;
  GRegex      *regex;

  if (pattern == NULL) {
    return NULL;
  }

  regex = g_regex_new (pattern, G_REGEX_OPTIMIZE, 0, NULL);
  if (regex == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_REGEX_PATTERN_FAIL, 0, NULL);
    // fprintf (stderr, "ERR: failed to compile %s\n", pattern);
    return NULL;
  }
  rx = malloc (sizeof (mkc_regex_t));
  if (rx == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  rx->regex = regex;
  return rx;
}

void
mkc_regex_free (mkc_regex_t *rx)
{
  if (rx == NULL) {
    return;
  }

  if (rx->regex != NULL) {
    g_regex_unref (rx->regex);
  }
  free (rx);
}

/* the caller must free the allocated string */
MKC_NODISCARD
char *
mkc_regex_escape (const char *str)
{
  char    *p;

  if (str == NULL) {
    return NULL;
  }
  p = g_regex_escape_string (str, -1);
  return p;
}

bool
mkc_regex_match (mkc_regex_t *rx, const char *str)
{
  if (rx == NULL || str == NULL) {
    return NULL;
  }
  return g_regex_match (rx->regex, str, 0, NULL);
}


char **
mkc_regex_get (mkc_regex_t *rx, const char *str)
{
  char **val;

  if (rx == NULL || str == NULL) {
    return NULL;
  }

  val = g_regex_split (rx->regex, str, 0);
  return val;
}

void
mkc_regex_get_free (char **val)
{
  g_strfreev (val);
}

MKC_NODISCARD
char *
mkc_regex_replace (mkc_regex_t *rx, const char *str, const char *repl)
{
  char  *nstr = NULL;
  /* G_REGEX_MATCH_DEFAULT is not defined */
  nstr = g_regex_replace (rx->regex, str, -1, 0, repl, 0, NULL);
  return nstr;
}

/* this routine re-creates the regex every time */
/* if it is re-used, consider creating a normal regex */
MKC_NODISCARD
char *
mkc_regex_replace_literal (const char *str, const char *tgt,
    const char *repl, mkc_error_t *mkcerr)
{
  mkc_regex_t  *rx;
  char        *nstr = NULL;
  char        *tmptgt;
  size_t      len;

  len = strlen (tgt) + 5;
  tmptgt = malloc (len);
  if (tmptgt == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  snprintf (tmptgt, len, "\\Q%s\\E", tgt);
  rx = mkc_regex_init (tmptgt, mkcerr);
  if (rx != NULL) {
    /* G_REGEX_MATCH_DEFAULT is not defined */
    nstr = g_regex_replace_literal (rx->regex, str, -1, 0, repl, 0, NULL);
    free (tmptgt);
    mkc_regex_free (rx);
  }
  return nstr;
}

#endif /* _package_glib */
