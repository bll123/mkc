/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

#include "ast.h"
#include "mkc_error.h"
#include "fileop.h"
#include "mkc_lex.h"
#include "mkc_log.h"
#include "parse.h"
#include "strutil.h"

typedef struct parse_t {
  /* temporary buffer variable */
  YY_BUFFER_STATE     buffer;
  /* temporary for string scanning */
  YY_BUFFER_STATE     obuffer;
  mkcyyscan_t         scanner;
  astmain_t       *astmain;
  mkc_error_t         *mkcerr;
  mkc_log_t           *log;
  char                *filename;
} parse_t;

MKC_NODISCARD
parse_t *
parse_init (astmain_t *astmain, mkc_log_t *log,
    mkc_error_t *mkcerr)
{
  parse_t   *parse;

  parse = malloc (sizeof (parse_t));
  if (parse == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }
  parse->astmain = astmain;
  parse->log = log;
  parse->mkcerr = mkcerr;
  parse->obuffer = NULL;
  parse->buffer = NULL;
  parse->filename = NULL;

  mkcyylex_init (&parse->scanner);

  return parse;
}

void
parse_free (parse_t *parse)
{
  if (parse == NULL) {
    return;
  }

  datafree (parse->filename);
  mkcyylex_destroy (parse->scanner);
  free (parse);
}

int
parse_start (parse_t *parse, FILE *fh)
{
  if (parse == NULL) {
    return MKC_ERR_FAILURE;
  }
  if (fh == NULL) {
    mkc_error_set (parse->mkcerr, MKC_ERR_NULL_ARGUMENT, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  parse->buffer = mkcyy_create_buffer (fh, YY_BUF_SIZE, parse->scanner);
  mkcyypush_buffer_state (parse->buffer, parse->scanner);

  return MKC_OK;
}

void *
parse_get_scanner (parse_t *parse)
{
  if (parse == NULL) {
    return NULL;
  }

  return parse->scanner;
}

void
parse_set_filename (parse_t *parse, const char *fname)
{
  const char    *p;

  if (parse == NULL) {
    return;
  }

  datafree (parse->filename);
  p = strrchr (fname, '/');
  if (p != NULL) {
    p += 1;
    parse->filename = strdup (p);
  } else {
    parse->filename = strdup (fname);
  }
  mkc_log_set_disp_filename (parse->log, parse->filename);
}

const char *
parse_get_filename (parse_t *parse)
{
  if (parse == NULL) {
    return NULL;
  }

  return parse->filename;
}

