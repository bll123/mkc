/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#ifndef MKC_BOOTSTRAP
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
#include "mkc_const.h"
#include "mkc_def.h"
#include "mkc_dirop.h"
#include "mkc_env.h"
#include "mkc_error.h"
#include "mkc_fileop.h"
#include "mkc_log.h"
#include "mkc_parse.h"
#include "mkc_path.h"
#include "mkc_profile.h"
#include "mkc_string.h"
#include "mkc_tmutil.h"

typedef struct {
  int       nargc;
  char      **utf8argv;
} argcopy_t;

static void copyargs (argcopy_t *argcopy, int argc, char *argv [], mkc_error_t *mkcerr);
static void cleanargs (argcopy_t *argcopy);
static mkc_err_code_t mkc_cleanup (mkc_astmain_t *astmain, argcopy_t *argcopy, mkc_log_t *log, mkc_error_t *error);
static int mkc_main_set_dflt_profile (const char *profnm, mkc_error_t *mkcerr);
void mkc_main_set_home (void);
void mkc_main_set_exec_path (argcopy_t *argcopy);
static void mkc_main_print_version (void);

int
main (int argc, char *argv [])
{
  argcopy_t       argcopy;
  mkc_option_t    mkcoptions;
  int             c;
  int             option_index;
  int             fnidx;
  mkc_parse_t     * parse = NULL;
  FILE            * fh = NULL;
  mkc_astmain_t   * astmain = NULL;
  mkc_error_t     * mkcerr = NULL;
  mkc_log_t       * log = NULL;
  mstime_t        starttm;
  mstime_t        proctm;
  time_t          etm;
  bool            loadcache = true;
  bool            debug = false;
  int             rc = 0;
  char            tbuff [MKC_PATH_MAX];
  char            cachename [MKC_PATH_MAX];

  static struct option mkc_cli_opts [] = {
    { "compiler",             required_argument,  NULL, 3   },
    { "mkc-dir",              required_argument,  NULL, 5   },
    { "no-cache",             no_argument,        NULL, 1   },
    { "parsedebug",           no_argument,        NULL, 2   },
    { "prefix",               required_argument,  NULL, 6   },
    { "profile",              required_argument,  NULL, 'p' },
    { "retest",               no_argument,        NULL, 'r' },
    { "set-default-profile",  required_argument,  NULL, 7   },
    { "version",              no_argument,        NULL, 'v'   },
    { NULL,                   no_argument,        NULL, 0   },
  };

  mkcerr = mkc_error_init ();
  copyargs (&argcopy, argc, argv, mkcerr);
  if (mkc_error_chk_err (mkcerr)) {
    rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
    return rc;
  }

  mkc_main_set_home ();
  mkc_main_set_exec_path (&argcopy);

  mkcoptions.dfltprofile = strdup (MKC_PROF_DEFAULT_NAME);
  mkcoptions.compilertxt = NULL;
  mkcoptions.stage = NULL;
  mkcoptions.prefix = NULL;
  mkcoptions.retest = false;

  mkc_path_build (MKC_PATH_CONFIG, tbuff, sizeof (tbuff),
      "defaultprofile.txt", mkcerr);
  fh = mkc_fopen (tbuff, "r");
  if (fh != NULL) {
    *tbuff = '\0';
    fgets (tbuff, sizeof (tbuff), fh);
    datafree (mkcoptions.dfltprofile);
    mkcoptions.dfltprofile = strdup (tbuff);
    fclose (fh);
  }

  while ((c = getopt_long_only (argcopy.nargc, argcopy.utf8argv,
      "p:rv", mkc_cli_opts, &option_index)) != -1) {
    switch (c) {
      case 'p': {
        if (optarg != NULL) {
          datafree (mkcoptions.dfltprofile);
          mkcoptions.dfltprofile = strdup (argcopy.utf8argv [optind - 1]);
        }
        break;
      }
      case 'r': {
        mkcoptions.retest = true;
        break;
      }
      case 'v': {
        mkc_main_print_version ();
        exit (0);
        break;
      }
      case 1: {
        loadcache = false;
        break;
      }
      case 2: {
        debug = 1;
        break;
      }
      case 3: {
        if (optarg != NULL) {
          mkcoptions.compilertxt = argcopy.utf8argv [optind - 1];
        }
        break;
      }
      case 5: {
        if (optarg != NULL) {
          mkc_path_set_dir (MKC_DIR_MKC_FILES, argcopy.utf8argv [optind - 1]);
        }
        break;
      }
      case 6: {
        break;
      }
      case 7: {
        if (optarg != NULL) {
          rc = mkc_main_set_dflt_profile (argcopy.utf8argv [optind - 1], mkcerr);
          rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
          datafree (mkcoptions.dfltprofile);
          exit (rc);
        }
        break;
      }
      default: {
        break;
      }
    }
  }

  /* create the mkc_files temporary directory tree */
  mkc_path_build (MKC_PATH_MKC_TMP, tbuff, sizeof (tbuff), NULL, mkcerr);
  rc = mkc_dirop_make (tbuff, mkcerr);
  if (rc != 0) {
    rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
    datafree (mkcoptions.dfltprofile);
    return rc;
  }

  log = mkc_log_init (mkcerr);
  mkc_path_build (MKC_PATH_MKC_FILES, tbuff, sizeof (tbuff),
      "log-mkc.txt", mkcerr);
//  mkc_log_open (log, tbuff, MKC_LOG_NORMAL);
  mkc_log_open (log, tbuff, MKC_LOG_ALL);

  fnidx = optind;
  if (fnidx >= argcopy.nargc) {
    fprintf (stderr, "no file specified.\n");
    rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
    datafree (mkcoptions.dfltprofile);
    return rc;
  }

  if (fnidx < argcopy.nargc) {
    fh = mkc_fopen (argcopy.utf8argv [fnidx], "r");
    if (fh == NULL) {
      mkc_error_set (mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, argcopy.utf8argv [fnidx]);
      rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
      datafree (mkcoptions.dfltprofile);
      return rc;
    }
  }

  astmain = mkc_ast_init (log, &mkcoptions, mkcerr);
  if (mkc_error_chk_err (mkcerr)) {
    rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
    datafree (mkcoptions.dfltprofile);
    return rc;
  }

  mstimestart (&starttm);
  parse = mkc_parse_init (astmain, log, mkcerr);
  mkc_parse_debug (parse, debug);

  if (! loadcache) {
    mkc_message ("-- cache disabled by user\n");
  }

  mkc_path_build (MKC_PATH_MKC_FILES, cachename, sizeof (cachename), "cache.mkc", mkcerr);

  if (loadcache) {
    time_t    cachetm;
    time_t    mkctm;

    cachetm = mkc_file_modtime (cachename);
    mkctm = mkc_file_modtime (argcopy.utf8argv [fnidx]);
    if (mkctm >= cachetm) {
      loadcache = false;
      mkc_message ("-- cache out of date\n");
      mkc_log (log, MKC_LOG_AST_PROCESS, "-- cache out of date\n");
    }
  }

  mkc_parse_start (parse, fh);
  if (mkc_error_chk_err (mkcerr)) {
    rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
    datafree (mkcoptions.dfltprofile);
    return rc;
  }

  if (loadcache) {
    FILE    *cfh;

    cfh = mkc_fopen (cachename, "r");
    if (cfh == NULL) {
      mkc_error_set (mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, cachename);
      rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
      datafree (mkcoptions.dfltprofile);
      return rc;
    }

    mkc_message ("-- loading cache\n");
    mkc_parse_start (parse, cfh);
    mkc_parse_set_filename (parse, cachename);
  }

  mkc_parse_set_filename (parse, argcopy.utf8argv [fnidx]);
  rc = mkc_parse (parse, mkc_parse_get_scanner (parse), astmain, mkcerr);

  mkc_parse_free (parse);

  if (mkc_error_chk_err (mkcerr)) {
    rc = mkc_cleanup (astmain, &argcopy, log, mkcerr);
    datafree (mkcoptions.dfltprofile);
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
    mkc_error_set (mkcerr, MKC_ERR_PARSE_FAILURE, 0, NULL);
  }

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
  datafree (mkcoptions.dfltprofile);
  return rc;
}

/* internal routines */

static void
copyargs (argcopy_t *argcopy, int argc, char *argv [], mkc_error_t *mkcerr)
{
#if _function_GetCommandLineW || (MKC_BOOTSTRAP && MKC_SYS_WIN)
  wchar_t         **wargv;
  int             targc;
#endif

  argcopy->nargc = argc;
  argcopy->utf8argv = NULL;
  if (argc > 0) {
    argcopy->utf8argv = malloc (sizeof (char *) * argc);
    if (argcopy->utf8argv == NULL) {
      mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    }
  }
#if _function_GetCommandLineW || (MKC_BOOTSTRAP && MKC_SYS_WIN)
  wargv = CommandLineToArgvW (GetCommandLineW(), &targc);
  for (int i = 0; i < argcopy->nargc; ++i) {
    argcopy->utf8argv [i] = mkc_fromwide (wargv [i]);
  }
  LocalFree (wargv);
#else
  for (int i = 0; i < argcopy->nargc; ++i) {
    argcopy->utf8argv [i] = strdup (argv [i]);
    if (argcopy->utf8argv [i] == NULL) {
      mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    }
  }
#endif
}

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

static mkc_err_code_t
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

static int
mkc_main_set_dflt_profile (const char *profnm, mkc_error_t *mkcerr)
{
  FILE  *fh;
  char  tbuff [MKC_PATH_MAX];
  int   rc = 0;

  mkc_path_build (MKC_PATH_CONFIG, tbuff, sizeof (tbuff), NULL, mkcerr);
  if (! mkc_is_directory (tbuff)) {
    rc = mkc_dirop_make (tbuff, mkcerr);
    if (rc != 0) {
      return rc;
    }
  }
  mkc_path_build (MKC_PATH_CONFIG, tbuff, sizeof (tbuff),
      "defaultprofile.txt", mkcerr);
  fh = mkc_fopen (tbuff, "w");
  if (fh != NULL) {
    fprintf (fh, "%s", profnm);
    fclose (fh);
  }
  return rc;
}

void
mkc_main_set_home (void)
{
  char    tbuff [MKC_PATH_MAX];

#if MKC_SYS_WIN
  mkc_env_get ("USERPROFILE", tbuff, sizeof (tbuff));
  mkc_normalize_path (tbuff, sizeof (tbuff));
#else
  mkc_env_get ("HOME", tbuff, sizeof (tbuff));
#endif
  mkc_path_set_dir (MKC_DIR_HOME, tbuff);
}

void
mkc_main_set_exec_path (argcopy_t *argcopy)
{
  char    tbuff [MKC_PATH_MAX];
  char    *p;

  stpecpy (tbuff, tbuff + sizeof (tbuff), argcopy->utf8argv [0]);
  mkc_normalize_path (tbuff, sizeof (tbuff));
  p = strrchr (tbuff, '/');
  if (p != NULL) {
    *p = '\0';
  }
  mkc_path_set_dir (MKC_DIR_EXEC, tbuff);

  p = strrchr (tbuff, '/');
  if (p != NULL) {
    *p = '\0';
  }
  mkc_path_set_dir (MKC_DIR_PREFIX, tbuff);
}

static void
mkc_main_print_version (void)
{
#if defined (VERSION)
  fprintf (stdout, "mkc version %s\n", VERSION);
#endif
}
