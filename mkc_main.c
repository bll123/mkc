/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#if ! MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

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
mkc_err_code_t mkc_cleanup (mkc_astmain_t *astmain, argcopy_t *argcopy, mkc_log_t *log, mkc_error_t *error);

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
  const char      * comparg = NULL;
  mkc_astmain_t   * astmain = NULL;
  yyscan_t        scanner;
  mkc_error_t     * mkcerr = NULL;
  mkc_log_t       * log = NULL;
  mstime_t        starttm;
  mstime_t        proctm;
  time_t          etm;
  char            tbuff [40];
  bool            loadcache = true;
  int             rc = 0;
  time_t          cachetm;
  time_t          mkctm;

  static struct option mkc_options [] = {
    { "compiler",       required_argument,  NULL,   3, },
    { "no-cache",       no_argument,        NULL,   1, },
    { "parsedebug",     no_argument,        NULL,   2 },
    { "profile",        required_argument,  NULL,   'p' },
    { NULL,             no_argument,        NULL,   0 },
  };

  mkcerr = mkc_error_init ();
  argcopy.nargc = argc;
  argcopy.utf8argv = NULL;
  if (argc > 0) {
    argcopy.utf8argv = malloc (sizeof (char *) * argc);
  }
#if _lib_GetCommandLineW || (MKC_BOOTSTRAP && _WIN32)
  wargv = CommandLineToArgvW (GetCommandLineW(), &targc);
  for (int i = 0; i < argcopy.nargc; ++i) {
    argcopy.utf8argv [i] = mkc_fromwide (wargv [i]);
  }
  LocalFree (wargv);
#else
  for (int i = 0; i < argcopy.nargc; ++i) {
    argcopy.utf8argv [i] = strdup (argv [i]);
  }
#endif

  while ((c = getopt_long_only (argcopy.nargc, argcopy.utf8argv, "p:",
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
      case 3: {
        if (optarg != NULL) {
          comparg = argcopy.utf8argv [optind - 1];
        }
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

  fnidx = optind;
  if (fnidx >= argcopy.nargc) {
    fprintf (stderr, "no file specified.\n");
    rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
    return rc;
  }

  if (fnidx < argcopy.nargc) {
    fh = mkc_fopen (argcopy.utf8argv [fnidx], "r");
    if (fh == NULL) {
      fprintf (stderr, "%s: %s\n", argcopy.utf8argv [fnidx], strerror (errno));
      mkc_error_set (mkcerr, MKC_ERR_FILE_NOT_FOUND);
      rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
      return rc;
    }
  }

  astmain = mkc_ast_init (log, dfltprof, comparg, mkcerr);
  if (mkc_error_chk_err (mkcerr)) {
    rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
    return rc;
  }

  mstimestart (&starttm);
  yylex_init (&scanner);

  cachetm = mkc_file_modtime ("mkc_files/cache.mkc");
  mkctm = mkc_file_modtime (argcopy.utf8argv [fnidx]);
  if (mkctm >= cachetm) {
    loadcache = false;
    mkc_message ("-- cache out of date\n");
    mkc_log (log, MKC_LOG_AST_PROCESS, "-- cache out of date\n");
  }

  if (loadcache) {
    FILE    *cachefh;

    cachefh = mkc_fopen ("mkc_files/cache.mkc", "r");
    if (cachefh != NULL) {
      mkc_message ("-- loading cache\n");
      mkc_log (log, MKC_LOG_AST_PROCESS, "-- loading cache\n");

      rc = mkc_parse (cachefh, scanner, astmain, log, dfltprof, mkcerr);
      fclose (cachefh);

      if (mkc_error_chk_err (mkcerr)) {
        rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
        return rc;
      }
      mkc_log (log, MKC_LOG_AST_PROCESS, "cache load complete\n");
    }
  }

  rc = mkc_parse (fh, scanner, astmain, log, dfltprof, mkcerr);
  if (mkc_error_chk_err (mkcerr)) {
    rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
    return rc;
  }

  etm = mstimeend (&starttm);
  mkc_elapsed_disp (etm, tbuff, sizeof (tbuff));
  mkc_message ("-- parse: %s\n", tbuff);
  mkc_log (log, MKC_LOG_STATISTICS, "-- parse: %s\n", tbuff);
  mstimestart (&proctm);

  if (rc == 0) {
    rc = mkc_ast_start (astmain);
  } else {
    mkc_error_set (mkcerr, MKC_ERR_PARSE_FAILURE);
  }

  yylex_destroy (scanner);

  if (fh != stdin) {
    fclose (fh);
  }

  etm = mstimeend (&proctm);
  mkc_elapsed_disp (etm, tbuff, sizeof (tbuff));
  mkc_message ("-- process: %s\n", tbuff);
  mkc_log (log, MKC_LOG_STATISTICS, "-- process: %s\n", tbuff);
  etm = mstimeend (&starttm);
  mkc_elapsed_disp (etm, tbuff, sizeof (tbuff));
  mkc_message ("-- total time: %s\n", tbuff);
  mkc_log (log, MKC_LOG_STATISTICS, "-- total time: %s\n", tbuff);

  rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
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
  int                 rc = 0;
  YY_BUFFER_STATE     state;

  state = yy_create_buffer (fh, YY_BUF_SIZE, scanner);
  yy_switch_to_buffer (state, scanner);
  rc = yyparse (scanner, astmain);

  yy_delete_buffer (state, scanner);

  return rc;
}

mkc_err_code_t
mkc_cleanup (mkc_astmain_t *astmain, argcopy_t *argcopy,
    mkc_log_t *log, mkc_error_t *mkcerr)
{
  mkc_err_code_t  rc;

  mkc_ast_free (astmain);
  cleanargs (argcopy);
  mkc_log_free (log);
  mkc_error_print (mkcerr);
  rc = mkc_error_value (mkcerr);
  mkc_error_free (mkcerr);

  return rc;
}
