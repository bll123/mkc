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
#include <string.h>
#include <errno.h>

#if _package_pcre

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_regex.h"
#include "mkc_nodiscard.h"

typedef struct mkc_regex_t {
  pcre2_code  *regex;
  mkc_error_t *mkcerr;
  uint32_t    opt;
  bool        jit;
} mkc_regex_t;

static void mkc_regex_error_set (mkc_error_t *mkcerr, mkc_err_code_t err, int perrcode);

MKC_NODISCARD
mkc_regex_t *
mkc_regex_init (const char *pattern, mkc_error_t *mkcerr)
{
  mkc_regex_t   *rx;
  pcre2_code    *regex;
  int           perrcode;
  size_t        perroffset;
  int           rc;

  if (pattern == NULL) {
    return NULL;
  }

  rx = malloc (sizeof (mkc_regex_t));
  if (rx == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  rx->opt = 0;
  rx->regex = NULL;
  rx->mkcerr = mkcerr;
  rx->jit = false;

  regex = pcre2_compile ((PCRE2_SPTR8) pattern, PCRE2_ZERO_TERMINATED,
      rx->opt, &perrcode, &perroffset, NULL);
  if (regex == NULL) {
    mkc_regex_error_set (mkcerr, MKC_ERR_REGEX_PATTERN_FAIL, perrcode);
    free (rx);
    return NULL;
  }

  rc = pcre2_jit_compile (regex, PCRE2_JIT_COMPLETE);
  if (rc == 0) {
    rx->jit = true;
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
    pcre2_code_free (rx->regex);
  }
  free (rx);
}

MKC_NODISCARD
char *
mkc_regex_escape (const char *str)
{
  /* not implemented */
  return NULL;
}

bool
mkc_regex_match (mkc_regex_t *rx, const char *str)
{
  int               rc;
  size_t            len;
  pcre2_match_data  *mdata;

  if (rx == NULL) {
    return NULL;
  }
  if (str == NULL) {
    mkc_error_set (rx->mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
    return NULL;
  }

  len = strlen (str);
  mdata = pcre2_match_data_create_from_pattern (rx->regex, NULL);

  if (rx->jit == false) {
    rc = pcre2_match (rx->regex, (PCRE2_SPTR8) str, len,
        0, rx->opt, mdata, NULL);
  } else {
    rc = pcre2_jit_match (rx->regex, (PCRE2_SPTR8) str, len,
        0, rx->opt, mdata, NULL);
  }
  if (rc < 0 && rc != PCRE2_ERROR_NOMATCH) {
    mkc_regex_error_set (rx->mkcerr, MKC_ERR_REGEX_MATCH_FAIL, rc);
  }

  pcre2_match_data_free (mdata);

  return rc;
}

/* important: this routine is not robust, and is meant for simple */
/* regular expressions */
int
mkc_regex_match_count (mkc_regex_t *rx, const char *str)
{
  int               mcount = 0;
  pcre2_match_data  *mdata;
  int               rc = 0;
  size_t            startoffset = 0;
  size_t            len;

  if (rx == NULL) {
    return mcount;
  }
  if (str == NULL) {
    mkc_error_set (rx->mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
    return mcount;
  }

  mdata = pcre2_match_data_create_from_pattern (rx->regex, NULL);
  len = strlen (str);

  while (true) {
    PCRE2_SIZE  *ov;

    if (rx->jit == false) {
      rc = pcre2_match (rx->regex, (PCRE2_SPTR8) str, len,
          startoffset, rx->opt, mdata, NULL);
    } else {
      rc = pcre2_jit_match (rx->regex, (PCRE2_SPTR8) str, len,
          startoffset, rx->opt, mdata, NULL);
    }
    if (rc == PCRE2_ERROR_NOMATCH) {
      break;
    }
    if (rc < 0 && rc != PCRE2_ERROR_NOMATCH) {
      mkc_regex_error_set (rx->mkcerr, MKC_ERR_REGEX_MATCH_FAIL, rc);
      pcre2_match_data_free (mdata);
      return 0;
    }

    ov = pcre2_get_ovector_pointer (mdata);
    startoffset = ov [1];
    if (startoffset >= len) {
      break;
    }
    mcount += 1;
  }
  pcre2_match_data_free (mdata);

  return mcount;
}


char **
mkc_regex_get (mkc_regex_t *rx, const char *str, int *mcount)
{
  pcre2_match_data  *mdata;
  PCRE2_UCHAR8      **val = NULL;
  int               rc = 0;
  size_t            len;

  *mcount = 0;

  if (rx == NULL) {
    return NULL;
  }
  if (str == NULL || mcount == NULL) {
    mkc_error_set (rx->mkcerr, MKC_ERR_INVALID_ARGUMENT, 0, NULL);
    return NULL;
  }

  mdata = pcre2_match_data_create_from_pattern (rx->regex, NULL);
  len = strlen (str);

  if (rx->jit == false) {
    rc = pcre2_match (rx->regex, (PCRE2_SPTR8) str, len,
        0, rx->opt, mdata, NULL);
  } else {
    rc = pcre2_jit_match (rx->regex, (PCRE2_SPTR8) str, len,
        0, rx->opt, mdata, NULL);
  }
  if (rc == PCRE2_ERROR_NOMATCH) {
    pcre2_match_data_free (mdata);
    return NULL;
  }

  if (rc < 0 && rc != PCRE2_ERROR_NOMATCH) {
    mkc_regex_error_set (rx->mkcerr, MKC_ERR_REGEX_MATCH_FAIL, rc);
    pcre2_match_data_free (mdata);
    return NULL;
  }

  pcre2_substring_list_get (mdata, &val, NULL);
  pcre2_match_data_free (mdata);

  if (val != NULL) {
    while (val [*mcount] != NULL) {
      *mcount += 1;
    }
  }

  return (char **) val;
}

void
mkc_regex_get_free (char **val)
{
  pcre2_substring_list_free ((PCRE2_UCHAR8 **) val);
}

MKC_NODISCARD
char *
mkc_regex_replace (mkc_regex_t *rx, const char *str, const char *repl)
{
  char              *nstr = NULL;
  size_t            len;
  size_t            rlen;
  int               rc;

  len = strlen (str);
  /* it is unknown how much more space will be needed */
  nstr = malloc (len + MKC_SMALL_BUFF_SZ);
  if (nstr == NULL) {
    mkc_error_set (rx->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  rlen = len + MKC_SMALL_BUFF_SZ;
  rc = pcre2_substitute (rx->regex, (PCRE2_SPTR8) str, len,
      0, rx->opt, NULL, NULL, (PCRE2_SPTR8) repl, PCRE2_ZERO_TERMINATED,
      (PCRE2_UCHAR8 *) nstr, &rlen);
  if (rc < 0 && rc != PCRE2_ERROR_NOMATCH) {
    mkc_regex_error_set (rx->mkcerr, MKC_ERR_REGEX_MATCH_FAIL, rc);
  }

  return nstr;
}

/* this routine re-creates the regex every time */
/* if it is re-used, consider creating a normal regex */
MKC_NODISCARD
char *
mkc_regex_replace_literal (const char *str, const char *tgt,
    const char *repl, mkc_error_t *mkcerr)
{
  mkc_regex_t   *rx;
  char          *nstr = NULL;
  char          *tmptgt;
  size_t        len;

  len = strlen (tgt) + 5;
  tmptgt = malloc (len);
  if (tmptgt == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  snprintf (tmptgt, len, "\\Q%s\\E", tgt);
  rx = mkc_regex_init (tmptgt, mkcerr);
  if (rx != NULL) {
    size_t      len;
    size_t      rlen;
    int         rc;

    len = strlen (str);
    nstr = malloc (len + MKC_SMALL_BUFF_SZ);
    if (nstr == NULL) {
      mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return NULL;
    }
    rlen = len + MKC_SMALL_BUFF_SZ;
    rc = pcre2_substitute (rx->regex, (PCRE2_SPTR) str, len,
        0, rx->opt, NULL, NULL, (PCRE2_SPTR) repl, PCRE2_ZERO_TERMINATED,
        (PCRE2_UCHAR8 *) nstr, &rlen);
    if (rc < 0 && rc != PCRE2_ERROR_NOMATCH) {
      mkc_regex_error_set (mkcerr, MKC_ERR_REGEX_MATCH_FAIL, rc);
    }

    free (tmptgt);
    mkc_regex_free (rx);
  }
  return nstr;
}

static void
mkc_regex_error_set (mkc_error_t *mkcerr, mkc_err_code_t err, int perrcode)
{
  char    tbuff [MKC_VNAME_MAX];

  *tbuff = '\0';
  pcre2_get_error_message (perrcode, (PCRE2_UCHAR8 *) tbuff, sizeof (tbuff));
  mkc_error_set (mkcerr, err, 0, tbuff);
}

#endif /* _package_pcre */
