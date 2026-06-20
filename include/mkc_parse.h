/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#pragma once

#include <stdio.h>
#include <stdint.h>

#include "mkc_ast.h"
#include "mkc_log.h"
#include "mkc_error.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mkc_parse_t mkc_parse_t;

mkc_parse_t * mkc_parse_init (mkc_astmain_t *astmain, mkc_log_t *log, mkc_error_t *mkcerr);
void mkc_parse_free (mkc_parse_t *parse);
void mkc_parse_debug (mkc_parse_t *parse, bool debug);
int mkc_parse_file (mkc_parse_t *parse, const char *fn, int32_t lineno, int colno);
int mkc_parse (mkc_parse_t *parse, FILE *fh);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif
