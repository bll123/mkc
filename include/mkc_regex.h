/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *    (from ballroomdj4)
 */
#pragma once

#include "mkc_error.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mkc_regex_t mkc_regex_t;

MKC_NODISCARD mkc_regex_t * mkc_regex_init (const char *pattern, mkc_error_t *mkcerr);
void mkc_regex_free (mkc_regex_t *rx);
MKC_NODISCARD char * mkc_regex_escape (const char *str);
bool mkc_regex_match (mkc_regex_t *rx, const char *str);
int mkc_regex_match_count (mkc_regex_t *rx, const char *str);
MKC_NODISCARD char ** mkc_regex_get (mkc_regex_t *rx, const char *str, int *mcount);
void mkc_regex_get_free (char **val);
MKC_NODISCARD char * mkc_regex_replace (mkc_regex_t *rx, const char *str, const char *repl);
MKC_NODISCARD char * mkc_regex_replace_literal (const char *str, const char *tgt, const char *repl, mkc_error_t *mkcerr);

#if defined (__cplusplus) || defined (c_plusplus)
} /* extern C */
#endif
