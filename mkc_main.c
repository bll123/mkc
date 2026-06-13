/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#if __has_include (<windows.h>)
# define NOCRYPT 1
# define NOGDI 1
# include <windows.h>
#endif

#include "mkc_ast.h"
#include "mkc_error.h"
#include "mkc_fileop.h"
#include "mkc_grammar.h"
#include "mkc_lex.h"
#include "mkc_log.h"
#include "mkc_profile.h"
#include "mkc_util.h"
#include "mkc_string.h"

typedef struct {
  int       nargc;
  char      **utf8argv;
} argcopy_t;

static void cleanargs (argcopy_t *argcopy);
static int mkc_parse (FILE *fh, yyscan_t scanner, mkc_astmain_t *astmain, mkc_log_t *log, const char *dfltprof, mkc_error_t *mkcerr);

int
main (int argc, char *argv [])
{
  argcopy_t       argcopy;
#if _lib_GetCommandLineW || (MKC_BOOTSTRAP && _WIN32)
  wchar_t         **wargv;
  int             targc;
#endif
  int             c;
  int             option_index;
  int             fnidx;
  FILE            * fh = NULL;
  const char      * dfltprof = MKC_PROF_RELEASE_NAME;
  mkc_astmain_t   * astmain = NULL;
  yyscan_t        scanner;
  mkc_error_t     *mkcerr;
  mkc_log_t       * log = NULL;
  mstime_t        starttm;
  mstime_t        proctm;
  time_t          etm;
  char            tbuff [40];
  bool            loadcache = true;
  int             rc;

  static struct option mkc_options [] = {
    { "no-cache",       no_argument,        NULL,   1, },
    { "parsedebug",     no_argument,        NULL,   2 },
    { "profile",        required_argument,  NULL,   'p' },
  };

  mkcerr = mkc_error_init ();
  argcopy.nargc = argc;
  argcopy.utf8argv = NULL;
  if (argc > 0) {
    argcopy.utf8argv = malloc (sizeof (char *) * argc);
  }
#if _lib_GetCommandLineW || (MKC_BOOTSTRAP && _WIN32)
  wargv = CommandLineToArgvW (GetCommandLineW(), &targc);
  for (int i = 0; i < argc; ++i) {
    argcopy.utf8argv [i] = mkc_fromwide (wargv [i]);
  }
  LocalFree (wargv);
#else
  for (int i = 0; i < argc; ++i) {
    argcopy.utf8argv [i] = strdup (argv [i]);
  }
#endif

  while ((c = getopt_long_only (argc, argcopy.utf8argv, "p:",
      mkc_options, &option_index)) != -1) {
    switch (c) {
      case 'p': {
        if (optarg != NULL) {
          dfltprof = argcopy.utf8argv [optind - 1];
        }
        break;
      }
      case 1: {
        loadcache = false;
        break;
      }
      case 2: {
        yydebug = 1;
        break;
      }
      default: {
        break;
      }
    }
  }

  log = mkc_log_init (mkcerr);
//  mkc_log_open (log, "mkc_files/log-mkc.txt", MKC_LOG_NORMAL);
  mkc_log_open (log, "mkc_files/log-mkc.txt", MKC_LOG_ALL);

  fh = stdin;
  fnidx = optind;
  if (fnidx < argc) {
    fh = mkc_fopen (argv [fnidx], "r");
    if (fh == NULL) {
      fprintf (stderr, "%s: %s\n", argv [fnidx], strerror (errno));
      mkc_log_free (log);
      cleanargs (&argcopy);
      mkc_error_set (mkcerr, MKC_ERR_FILE_NOT_FOUND);
      mkc_error_print (mkcerr);
      rc = mkc_error_value (mkcerr);
      mkc_error_free (mkcerr);
      return rc;
    }
  }

  astmain = mkc_ast_init (log, dfltprof, mkcerr);
  if (mkc_error_chk_err (mkcerr)) {
    mkc_log_free (log);
    cleanargs (&argcopy);
    mkc_error_print (mkcerr);
    rc = mkc_error_value (mkcerr);
    mkc_error_free (mkcerr);
    return rc;
  }

  mstimestart (&starttm);
  yylex_init (&scanner);

  if (loadcache) {
    FILE    *cachefh;

    cachefh = mkc_fopen ("mkc_files/cache.mkc", "r");
    if (cachefh != NULL) {
      mkc_message ("-- loading cache\n");
      mkc_log (log, MKC_LOG_AST_PROCESS, "== loading cache\n", NULL);
      mkc_parse (cachefh, scanner, astmain, log, dfltprof, mkcerr);
      fclose (cachefh);
      if (mkc_error_chk_err (mkcerr)) {
        mkc_ast_free (astmain);
        mkc_log_free (log);
        cleanargs (&argcopy);
        mkc_error_print (mkcerr);
        rc = mkc_error_value (mkcerr);
        mkc_error_free (mkcerr);
        return rc;
      }
    }
  }

  mkc_parse (fh, scanner, astmain, log, dfltprof, mkcerr);
  if (mkc_error_chk_err (mkcerr)) {
    mkc_ast_free (astmain);
    mkc_log_free (log);
    cleanargs (&argcopy);
    mkc_error_print (mkcerr);
    rc = mkc_error_value (mkcerr);
    mkc_error_free (mkcerr);
    return rc;
  }
  yylex_destroy (scanner);
  mkc_ast_free (astmain);

  if (fh != stdin) {
    fclose (fh);
  }
  etm = mstimeend (&starttm);
  mkc_message ("-- parse: %s\n", mkc_elapsed_disp (etm, tbuff, sizeof (tbuff)));
  mkc_log (log, MKC_LOG_STATISTICS,
      "-- parse: %s\n", mkc_elapsed_disp (etm, tbuff, sizeof (tbuff)), NULL);
  mstimestart (&proctm);

  etm = mstimeend (&proctm);
  mkc_elapsed_disp (etm, tbuff, sizeof (tbuff));
  mkc_message ("-- process: %s\n", tbuff);
  mkc_log (log, MKC_LOG_STATISTICS, "-- process: %s\n", tbuff, NULL);
  etm = mstimeend (&starttm);
  mkc_elapsed_disp (etm, tbuff, sizeof (tbuff));
  mkc_message ("-- total time: %s\n", tbuff);
  mkc_log (log, MKC_LOG_STATISTICS, "-- total time: %s\n", tbuff, NULL);

  cleanargs (&argcopy);
  mkc_log_free (log);

  mkc_error_print (mkcerr);
  rc = mkc_error_value (mkcerr);
  mkc_error_free (mkcerr);
  return rc;
}

/* internal routines */

static void
cleanargs (argcopy_t *argcopy)
{
  if (argcopy->utf8argv != NULL) {
    for (int i = 0; i < argcopy->nargc; ++i) {
      if (argcopy->utf8argv [i] != NULL) {
        free (argcopy->utf8argv [i]);
      }
    }

    free (argcopy->utf8argv);
  }
}

static int
mkc_parse (FILE *fh, yyscan_t scanner, mkc_astmain_t *astmain,
    mkc_log_t *log, const char *dfltprof,
    mkc_error_t *mkcerr)
{
  int                 rc;
  YY_BUFFER_STATE     state;

  state = yy_create_buffer (fh, YY_BUF_SIZE, scanner);
  yy_switch_to_buffer (state, scanner);
  rc = yyparse (scanner, astmain);

  if (rc == 0) {
    rc = mkc_ast_start (astmain);
  } else {
    mkc_error_set (mkcerr, MKC_ERR_PARSE_FAILURE);
  }

  yy_delete_buffer (state, scanner);

  return rc;
}
