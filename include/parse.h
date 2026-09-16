/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_PARSE_H
#define INC_PARSE_H

#include <stdio.h>
#include <stdint.h>

#include "ast.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct parse_t parse_t;

/* parse.c */
MKC_NODISCARD parse_t * parse_init (astmain_t *astmain, mkc_log_t *log, mkc_error_t *mkcerr);
void parse_free (parse_t *parse);
void parse_debug (parse_t *parse, bool debug);
int parse_start (parse_t *parse, FILE *fh);
void * parse_get_scanner (parse_t *parse);
void parse_set_filename (parse_t *parse, const char *fname);
const char * parse_get_filename (parse_t *parse);

/* mkc_grammar.y */
int parse_process (parse_t *parse, void *scanner, astmain_t *astmain, mkc_error_t *mkcerr);
void parse_debug (parse_t *parse, bool debug);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_PARSE_H */
