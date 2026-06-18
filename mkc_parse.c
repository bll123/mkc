/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

#include "mkc_ast.h"
#include "mkc_error.h"
#include "mkc_fileop.h"
#include "mkc_grammar.h"
#include "mkc_lex.h"
#include "mkc_log.h"
#include "mkc_parse.h"

typedef struct mkc_parse_t {
  YY_BUFFER_STATE     state;
  mkcyyscan_t         scanner;
  mkc_astmain_t       *astmain;
  mkc_error_t         *mkcerr;
  mkc_log_t           *log;
} mkc_parse_t;

mkc_parse_t *
mkc_parse_init (mkc_astmain_t *astmain, mkc_log_t *log,
    mkc_error_t *mkcerr)
{
  mkc_parse_t   *parse;

  parse = malloc (sizeof (mkc_parse_t));
  if (parse == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY);
    return NULL;
  }
  parse->astmain = astmain;
  parse->log = log;
  parse->mkcerr = mkcerr;

  mkcyylex_init (&parse->scanner);

  return parse;
}

void
mkc_parse_free (mkc_parse_t *parse)
{
  if (parse == NULL) {
    return;
  }

  mkcyylex_destroy (parse->scanner);
  free (parse);
}

void
mkc_parse_debug (mkc_parse_t *parse, bool debug)
{
  if (parse == NULL) {
    return;
  }

  mkcyydebug = debug;
}

int
mkc_parse_file (mkc_parse_t *parse, const char *fn, int32_t lineno, int colno)
{
  FILE    *fh;
  int     rc = MKC_ERR_FILE_NOT_FOUND;

  if (parse == NULL) {
    return MKC_ERR_FAILURE;
  }
  if (fn == NULL) {
    mkc_error_set (parse->mkcerr, MKC_ERR_NULL_ARGUMENT);
    return MKC_ERR_FAILURE;
  }

  mkc_error_set_line_col (parse->mkcerr, lineno, colno);

  fh = mkc_fopen (fn, "r");
  if (fh != NULL) {
    rc = mkc_parse (parse, fh);
    fclose (fh);
  }

  return rc;
}

int
mkc_parse (mkc_parse_t *parse, FILE *fh)
{
  int               rc = 0;

  if (parse == NULL) {
    return MKC_ERR_FAILURE;
  }
  if (fh == NULL) {
    mkc_error_set (parse->mkcerr, MKC_ERR_NULL_ARGUMENT);
    return MKC_ERR_FAILURE;
  }

  parse->state = mkcyy_create_buffer (fh, YY_BUF_SIZE, parse->scanner);
  mkcyypush_buffer_state (parse->state, parse->scanner);
  rc = mkcyyparse (parse->scanner, parse, parse->astmain);
  if (rc == 1) {
    mkc_error_set (parse->mkcerr, MKC_ERR_PARSE_FAILURE);
  }
  if (rc == 2) {
    mkc_error_set (parse->mkcerr, MKC_ERR_OUT_OF_MEMORY);
  }
  mkcyypop_buffer_state (parse->scanner);

  return rc;
}

