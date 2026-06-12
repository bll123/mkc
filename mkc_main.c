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
#include "mkc_def.h"
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

int
main (int argc, char *argv [])
{
  argcopy_t           argcopy;
#if _lib_GetCommandLineW || (MKC_BOOTSTRAP && _WIN32)
  wchar_t       **wargv;
  int           targc;
#endif
  int                 c;
  int                 option_index;
  int                 fnidx;
  FILE                * fh = NULL;
  const char          * dfltprof = MKC_PROF_RELEASE_NAME;
  yyscan_t            scanner;
  YY_BUFFER_STATE     state;
  mkc_astmain_t       * astproc = NULL;
  int                 rc;
  mkc_error_t         mkcerr = MKC_OK;
  mkc_log_t           * log = NULL;
  mstime_t            starttm;
  mstime_t            proctm;
  time_t              etm;
  char                tbuff [40];

  static struct option mkc_options [] = {
    { "parsedebug",    no_argument,         NULL,   1 },
    { "profile",       required_argument,   NULL,   'p' },
  };

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
        yydebug = 1;
        break;
      }
      default: {
        break;
      }
    }
  }

  log = mkc_log_init (&mkcerr);
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
      return MKC_ERR_FILE_NOT_FOUND;
    }
  }

  mstimestart (&starttm);
  yylex_init (&scanner);
  state = yy_create_buffer (fh, YY_BUF_SIZE, scanner);
  yy_switch_to_buffer (state, scanner);
  astproc = mkc_ast_init (log, &mkcerr, dfltprof);
  if (mkcerr != MKC_OK) {
    mkc_ast_free (astproc);
    mkc_log_free (log);
    cleanargs (&argcopy);
    return mkcerr;
  }
  rc = yyparse (scanner, astproc);
  yy_delete_buffer (state, scanner);
  yylex_destroy (scanner);

  if (fh != stdin) {
    fclose (fh);
  }
  etm = mstimeend (&starttm);
  mkc_message ("-- parse: %s\n", mkc_elapsed_disp (etm, tbuff, sizeof (tbuff)));
  mstimestart (&proctm);

  if (rc == 0) {
    rc = mkc_ast_start (astproc);
  }
  mkc_ast_free (astproc);
  mkc_log_free (log);

  etm = mstimeend (&proctm);
  mkc_message ("-- process: %s\n", mkc_elapsed_disp (etm, tbuff, sizeof (tbuff)));
  etm = mstimeend (&starttm);
  mkc_message ("-- total time: %s\n", mkc_elapsed_disp (etm, tbuff, sizeof (tbuff)));
  cleanargs (&argcopy);
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
