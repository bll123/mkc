/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_PARSE_H
#define INC_MKC_PARSE_H

#include <stdio.h>
#include <stdint.h>

#include "mkc_ast.h"
#include "mkc_log.h"
#include "mkc_error.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mkc_parse_t mkc_parse_t;

/* mkc_parse.c */
mkc_parse_t * mkc_parse_init (mkc_astmain_t *astmain, mkc_log_t *log, mkc_error_t *mkcerr);
void mkc_parse_free (mkc_parse_t *parse);
void mkc_parse_debug (mkc_parse_t *parse, bool debug);
int mkc_parse_start (mkc_parse_t *parse, FILE *fh);
void mkc_parse_finish (mkc_parse_t *parse);
void * mkc_parse_get_scanner (mkc_parse_t *parse);

/* mkc_grammar.y */
int mkc_parse (mkc_parse_t *parse, void *scanner, mkc_astmain_t *astmain, mkc_error_t *mkcerr);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_PARSE_H */
