/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "alternate.h"
#include "attribute.h"
#include "compile.h"
#include "fileop.h"
#include "mkc_const.h"
#include "mkc_error.h"
#include "mkc_log.h"
#include "os_process.h"
#include "pathutil.h"
#include "strutil.h"

typedef struct compile_t {
  scopedvar_t       * scopedvar;
  mkc_attribute_t   * attr;
  mkc_error_t       * mkcerr;
  mkc_log_t         * log;
  chararr_t         * compflags;
  chararr_t         * addcompflags;
  chararr_t         * linkflags;
  chararr_t         * addlinkflags;
  chararr_t         * libs;
  chararr_t         * addlibs;
  chararr_t         * targv;
  mkc_compiler_t    compiler;
  bool              preprocess;
  bool              usetemplate;
} compile_t;

static char const * const MKC_C_TEST_HDR_LIST = "MKC_TV_TEST_HEADER_LIST";

typedef int (*test_func_t)(compile_t *comptest, mkc_compiler_t compiler, const char *fname, char *rbuff, size_t rsz);

static int compile_only (compile_t *comptest, mkc_compiler_t compiler, const char *fname, char *rbuff, size_t rsz);
static int compile_link (compile_t *comptest, mkc_compiler_t compiler, const char *fname, char *rbuff, size_t rsz);
static int compile_link_run (compile_t *comptest, mkc_compiler_t compiler, const char *fname, char *rbuff, size_t rsz);
static void compile_append_list_arg (compile_t *comptest, mkc_list_t *list);
static bool compile_append_flags (compile_t *comptest, chararr_t *flags);

compile_t *
compile_init (scopedvar_t *scopedvar,
    mkc_attribute_t *attr, mkc_log_t *log, mkc_error_t *mkcerr)
{
  compile_t    *comptest;

  comptest = malloc (sizeof (compile_t));
  if (comptest == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return NULL;
  }

  comptest->scopedvar = scopedvar;
  comptest->attr = attr;
  comptest->log = log;
  comptest->mkcerr = mkcerr;
  comptest->addcompflags = chararr_init (mkcerr);
  if (comptest->addcompflags == NULL) {
    return NULL;
  }
  comptest->addlinkflags = chararr_init (mkcerr);
  if (comptest->addlinkflags == NULL) {
    return NULL;
  }
  comptest->addlibs = chararr_init (mkcerr);
  if (comptest->addlibs == NULL) {
    return NULL;
  }

  comptest->targv = chararr_init (mkcerr);
  if (comptest->targv == NULL) {
    return NULL;
  }
  compile_reset (comptest);

  return comptest;
}

void
compile_free (compile_t *comptest)
{
  if (comptest == NULL) {
    return;
  }

  chararr_free (comptest->targv);
  chararr_free (comptest->addcompflags);
  chararr_free (comptest->addlinkflags);
  chararr_free (comptest->addlibs);
  free (comptest);
}

void
compile_set_flags (compile_t *comptest, chararr_t * compflags,
    chararr_t * linkflags, chararr_t * libs)
{
  if (comptest == NULL) {
    return;
  }

  comptest->compflags = compflags;
  comptest->linkflags = linkflags;
  comptest->libs = libs;
  return;
}

void
compile_set_compiler (compile_t *comptest, mkc_compiler_t compiler)
{
  if (comptest == NULL) {
    return;
  }

  comptest->compiler = compiler;
  return;
}

void
compile_preprocess (compile_t *comptest)
{
  if (comptest == NULL) {
    return;
  }

  comptest->preprocess = true;
  return;
}

void
compile_usetemplate (compile_t *comptest)
{
  if (comptest == NULL) {
    return;
  }

  comptest->usetemplate = true;
  return;
}

void
compile_reset (compile_t *comptest)
{
  if (comptest == NULL) {
    return;
  }

  comptest->compflags = NULL;
  comptest->linkflags = NULL;
  comptest->libs = NULL;

  chararr_reset (comptest->targv, 0);
  chararr_reset (comptest->addcompflags, 0);
  chararr_reset (comptest->addlinkflags, 0);
  chararr_reset (comptest->addlibs, 0);
  comptest->preprocess = false;
  comptest->usetemplate = false;
}

void
compile_create_header_var (compile_t *comptest)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;
  char            * hdrtxt = NULL;
  char            * tmp = NULL;
  size_t          hdrtxtlen = 1;
  mkc_alternate_t  * alt;


  alt = comptest->attr->curralt;
  mkc_list_iter_start (alt->hdrlist, &iteridx);
  while ((lidx = mkc_list_iter_next (alt->hdrlist, &iteridx)) != MKC_ITER_FINISH) {
    char        tbuff [MKC_PATH_MAX];
    value_t *lvalue;
    size_t      tlen;

    if (mkc_error_chk_err (comptest->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (alt->hdrlist, lidx);
    if (comptest->attr->headertype == MKC_HEADER_MODERN) {
      snprintf (tbuff, sizeof (tbuff),
          "#if __has_include (<%s>)\n"
          "# include <%s>\n"
          "#endif\n", lvalue->sval, lvalue->sval);
    } else {
      snprintf (tbuff, sizeof (tbuff), "#include <%s>\n", lvalue->sval);
    }
    tlen = strlen (tbuff);
    hdrtxtlen += tlen;
    hdrtxt = realloc (hdrtxt, hdrtxtlen);
    stpecpy (hdrtxt + hdrtxtlen - tlen - 1, hdrtxt + hdrtxtlen, tbuff);
  }

  tmp = hdrtxt;
  if (hdrtxt == NULL) {
    tmp = "";
  }
  scopedvar_set_str (comptest->scopedvar, SV_T_LOCAL, MKC_C_TEST_HDR_LIST, tmp, MKC_VCTXT_TEMP);

  free (hdrtxt);
}

const char *
compile_get_compstr (compile_t *comptest, mkc_compiler_t compiler,
    char *buff, size_t sz)
{
  const char    *envstr;
  value_t       *value;

  envstr = compiler_get_env_name (compiler);
  value = scopedvar_get_value (comptest->scopedvar, SV_T_INTERNAL, envstr);
  scopedvar_value_get_str (comptest->scopedvar, value, buff, sz);
  return buff;
}

void
compile_file_sub_copy (compile_t *comptest,
    char *tbuff, size_t sz,
    const char *fname, const char *origsfx, const char *sfx)
{
  char    tfn [MKC_VNAME_MAX];
  char    *fbuff;
  char    *data;
  char    *ndata;
  FILE    *fh;
  size_t  fsz;

  fbuff = malloc (MKC_PATH_MAX);
  if (fbuff == NULL) {
    mkc_error_set (comptest->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return;
  }

  snprintf (tfn, sizeof (tfn), "%s%s", fname, origsfx);
  path_build (MKC_PATH_MKC_TEMPLATES, fbuff, MKC_PATH_MAX, tfn, comptest->mkcerr);
  mkc_log (comptest->log, MKC_LOG_CHECK, "filename: %s\n", fbuff);
  data = fileop_read_file (fbuff, &fsz, comptest->mkcerr);
  if (mkc_error_chk_err (comptest->mkcerr)) {
    free (fbuff);
    return;
  }
  ndata = scopedvar_substitute (comptest->scopedvar, data, SV_NO_ESCAPE, 0);
  mkc_log (comptest->log, MKC_LOG_CHECK, "--- code:\n");
  mkc_log (comptest->log, MKC_LOG_CHECK, "%s", ndata);
  mkc_log (comptest->log, MKC_LOG_CHECK, "---\n");
  free (data);

  snprintf (tfn, sizeof (tfn), "%s%s", fname, sfx);
  path_build (MKC_PATH_MKCF_TMP, tbuff, sz, tfn, comptest->mkcerr);

  fh = fileop_open (tbuff, "wb");
  if (fh == NULL) {
    mkc_error_set (comptest->mkcerr, MKC_ERR_FILE_NOT_FOUND, errno, tbuff);
  } else {
    fwrite (ndata, strlen (ndata), 1, fh);
    fclose (fh);
  }
  free (ndata);
  free (fbuff);
}

int
compile_exec (compile_t *comptest, ct_type_t ctype,
    mkc_compiler_t compiler, const char *fname, char *rbuff, size_t rsz)
{
  int             rc = MKC_ERR_FAILURE;
  int             altsz;
  mkc_alternate_t * oldcurr;
  mkc_list_t      * alternates;
  mkc_alternate_t * alt;
  mkc_listidx_t   iteridx;
  mkc_listidx_t   aidx;
  test_func_t     func = NULL;
  bool            first = true;

  alternates = comptest->attr->alternates;
  altsz = mkc_list_size (alternates);

  oldcurr = comptest->attr->curralt;

  switch (ctype) {
    case MKC_COMPILE_ONLY: {
      func = compile_only;
      break;
    }
    case MKC_COMPILE_LINK: {
      func = compile_link;
      break;
    }
    case MKC_COMPILE_RUN: {
      func = compile_link_run;
      break;
    }
  }

  mkc_list_iter_start (alternates, &iteridx);
  while ((aidx = mkc_list_iter_next (alternates, &iteridx)) != MKC_ITER_FINISH) {
    alt = mkc_list_get_by_idx (alternates, aidx);

    if (first && altsz > 1) {
      /* if any alternates are specified, only test the alternates, */
      /* not the default */
      first = false;
      continue;
    }

    comptest->attr->curralt = alt;
    compile_create_header_var (comptest);
    rc = (*func) (comptest, compiler, fname, rbuff, rsz);

    if (rc == 0 && alt->name != NULL) {
      scopedvar_set_integer (comptest->scopedvar, SV_T_SEARCH, alt->name,
          rc == 0 ? true : false, MKC_VCTXT_CHECK);
    }

    if (rc == 0) {
      /* the first alternate that succeeds stops the test */
      break;
    }
  }

  comptest->attr->curralt = oldcurr;
  return rc;
}

void
compile_append_compflag (compile_t *comptest, const char *flag)
{
  if (comptest == NULL) {
    return;
  }

  chararr_append (comptest->addcompflags, flag);
}

void
compile_append_linkflag (compile_t *comptest, const char *flag)
{
  if (comptest == NULL) {
    return;
  }

  chararr_append (comptest->addlinkflags, flag);
}

/* internal routines */

static int
compile_only (compile_t *comptest, mkc_compiler_t compiler,
    const char *fname, char *rbuff, size_t rsz)
{
  int             rc;
  char            * tbuff;
  char            * compstr;
  char            * outfile;
  size_t          retsz;
  const char      * sfx = NULL;
  bool            rallocated = false;
  bool            cpreprocess = false;
  mkc_alternate_t * alt;

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    mkc_error_set (comptest->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }
  compstr = malloc (MKC_PATH_MAX);
  if (compstr == NULL) {
    mkc_error_set (comptest->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (tbuff);
    return MKC_ERR_FAILURE;
  }
  outfile = malloc (MKC_PATH_MAX);
  if (outfile == NULL) {
    mkc_error_set (comptest->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (tbuff);
    free (compstr);
    return MKC_ERR_FAILURE;
  }

  if (rbuff == NULL) {
    rsz = MKC_SMALL_BUFF_SZ;
    rbuff = malloc (rsz);
    if (rbuff == NULL) {
      mkc_error_set (comptest->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      free (tbuff);
      free (compstr);
      free (outfile);
      return MKC_ERR_FAILURE;
    }
    *rbuff = '\0';
    rallocated = true;
  }

  compile_get_compstr (comptest, compiler, compstr, MKC_PATH_MAX);
  chararr_append (comptest->targv, compstr);

  if (comptest->usetemplate) {
    sfx = compiler_get_suffix (compiler);
// ### will need to be fixed, the original suffix may change
    compile_file_sub_copy (comptest, tbuff, MKC_PATH_MAX, fname, ".c", sfx);
  } else {
    stpecpy (tbuff, tbuff + MKC_PATH_MAX, fname);
  }

  compile_append_flags (comptest, comptest->addcompflags);
  cpreprocess = compile_append_flags (comptest, comptest->compflags);

  alt = comptest->attr->curralt;
  compile_append_list_arg (comptest, alt->compflags);

  if (comptest->preprocess) {
    chararr_append (comptest->targv, "-E");
    cpreprocess = true;
  }
  if (! cpreprocess) {
    chararr_append (comptest->targv, "-c");
    chararr_append (comptest->targv, "-o");
    path_build (MKC_PATH_MKCF_TMP, outfile, MKC_PATH_MAX, "mkctest.o", comptest->mkcerr);
    chararr_append (comptest->targv, outfile);
  }
  chararr_append (comptest->targv, tbuff);
//  compile_append_list_arg (comptest, alt->linkflags);
  if (mkc_error_chk_err (comptest->mkcerr)) {
    free (tbuff);
    free (compstr);
    free (outfile);
    if (rallocated) {
      free (rbuff);
    }
    return MKC_ERR_FAILURE;
  }
  chararr_append (comptest->targv, NULL);

  mkc_log_chararr (comptest->log, "comp-only: cmd:", comptest->targv);

  rc = os_process_pipe (chararr_get_arr (comptest->targv),
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);

  if (retsz > 0) {
    mkc_log (comptest->log, MKC_LOG_CHECK, "--- compile log (%zd)\n", retsz);
    if (retsz < 2000) {
      mkc_log (comptest->log, MKC_LOG_CHECK, "%s", rbuff);
    } else {
      mkc_log (comptest->log, MKC_LOG_CHECK_VERBOSE, "%s\n", rbuff);
    }
    mkc_log (comptest->log, MKC_LOG_CHECK, "---\n");

    if (comptest->attr->printerrors) {
      fprintf (stderr, "%s", rbuff);
    }
  }
  mkc_log (comptest->log, MKC_LOG_CHECK, "  rc: %d\n", rc);

  free (tbuff);
  free (compstr);
  free (outfile);
  if (rallocated) {
    free (rbuff);
  }
  return rc;
}

static int
compile_link (compile_t *comptest, mkc_compiler_t compiler,
    const char *fname, char *rbuff, size_t rsz)
{
  int               rc;
  size_t            retsz;
  bool              rallocated = false;
  char              *compstr;
  char              *outfile;
  char              *objfile;
  mkc_alternate_t  * alt;


  if (rbuff == NULL) {
    rsz = MKC_SMALL_BUFF_SZ;
    rbuff = malloc (rsz);
    if (rbuff == NULL) {
      mkc_error_set (comptest->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return MKC_ERR_FAILURE;
    }
    *rbuff = '\0';
    rallocated = true;
  }

  rc = compile_only (comptest, compiler, fname, rbuff, rsz);
  chararr_reset (comptest->targv, 0);
  comptest->preprocess = false;
  if (rc != 0) {
    if (rallocated) {
      free (rbuff);
    }
    return rc;
  }

  compstr = malloc (MKC_PATH_MAX);
  if (compstr == NULL) {
    mkc_error_set (comptest->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }
  objfile = malloc (MKC_PATH_MAX);
  if (objfile == NULL) {
    mkc_error_set (comptest->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (compstr);
    return MKC_ERR_FAILURE;
  }
  outfile = malloc (MKC_PATH_MAX);
  if (outfile == NULL) {
    mkc_error_set (comptest->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    free (objfile);
    free (compstr);
    return MKC_ERR_FAILURE;
  }

  compile_get_compstr (comptest, compiler, compstr, MKC_PATH_MAX);
  chararr_append (comptest->targv, compstr);

  chararr_append (comptest->targv, "-o");
  path_build (MKC_PATH_MKCF_TMP, outfile, MKC_PATH_MAX, "mkctest.exe", comptest->mkcerr);
  chararr_append (comptest->targv, outfile);

  compile_append_flags (comptest, comptest->addlinkflags);
  compile_append_flags (comptest, comptest->linkflags);

  alt = comptest->attr->curralt;
  compile_append_list_arg (comptest, alt->linkflags);

  path_build (MKC_PATH_MKCF_TMP, objfile, MKC_PATH_MAX, "mkctest.o", comptest->mkcerr);
  chararr_append (comptest->targv, objfile);

  compile_append_flags (comptest, comptest->addlibs);
  compile_append_flags (comptest, comptest->libs);
  compile_append_list_arg (comptest, alt->libs);
  chararr_append (comptest->targv, NULL);

  if (mkc_error_chk_err (comptest->mkcerr)) {
    if (rallocated) {
      free (rbuff);
    }
    return MKC_ERR_FAILURE;
  }

  mkc_log_chararr (comptest->log, "link: cmd:", comptest->targv);

  rc = os_process_pipe (chararr_get_arr (comptest->targv),
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);

  mkc_log (comptest->log, MKC_LOG_CHECK, "  rc: %d\n", rc);
  if (retsz > 0) {
    mkc_log (comptest->log, MKC_LOG_CHECK, "--- link log\n");
    mkc_log (comptest->log, MKC_LOG_CHECK, "%s\n", rbuff);
    mkc_log (comptest->log, MKC_LOG_CHECK, "---\n");
  }

  free (objfile);
  free (outfile);
  free (compstr);
  if (rallocated) {
    free (rbuff);
  }

  return rc;
}

static int
compile_link_run (compile_t *comptest, mkc_compiler_t compiler,
    const char *fname, char *rbuff, size_t rsz)
{
  int         rc;
  bool        rallocated = false;
  size_t      retsz;
  char        *exefile;

  rc = compile_link (comptest, compiler, fname, NULL, 0);
  comptest->preprocess = false;
  chararr_reset (comptest->targv, 0);
  if (rc != 0) {
    return rc;
  }

  exefile = malloc (MKC_PATH_MAX);
  if (exefile == NULL) {
    mkc_error_set (comptest->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
    return MKC_ERR_FAILURE;
  }

  chararr_reset (comptest->targv, 0);
  path_build (MKC_PATH_MKCF_TMP, exefile, MKC_PATH_MAX, "mkctest.exe", comptest->mkcerr);
  chararr_append (comptest->targv, exefile);
  if (mkc_error_chk_err (comptest->mkcerr)) {
    free (exefile);
    return MKC_ERR_FAILURE;
  }
  chararr_append (comptest->targv, NULL);

  mkc_log_chararr (comptest->log, "run: cmd:", comptest->targv);

  if (rbuff == NULL) {
    rsz = MKC_SMALL_BUFF_SZ;
    rbuff = malloc (rsz);
    if (rbuff == NULL) {
      mkc_error_set (comptest->mkcerr, MKC_ERR_OUT_OF_MEMORY, 0, NULL);
      return MKC_ERR_FAILURE;
    }
    rallocated = true;
  }

  rc = os_process_pipe (chararr_get_arr (comptest->targv),
      OS_PROC_WAIT | OS_PROC_NOWINDOW, rbuff, rsz, &retsz);

  mkc_log (comptest->log, MKC_LOG_CHECK, "  run: rc: %d\n", rc);
  if (retsz > 0) {
    mkc_log (comptest->log, MKC_LOG_CHECK, "--- run log\n");
    mkc_log (comptest->log, MKC_LOG_CHECK, "%s", rbuff);
    mkc_log (comptest->log, MKC_LOG_CHECK, "---\n");
  }

  free (exefile);
  if (rallocated) {
    free (rbuff);
  }

  return rc;
}

static void
compile_append_list_arg (compile_t *comptest, mkc_list_t *list)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;

  if (comptest == NULL || list == NULL) {
    return;
  }

  mkc_list_iter_start (list, &iteridx);
  while ((lidx = mkc_list_iter_next (list, &iteridx)) != MKC_ITER_FINISH) {
    value_t   *lvalue;

    if (mkc_error_chk_err (comptest->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (list, lidx);
    chararr_append (comptest->targv, lvalue->sval);
  }
}

static bool
compile_append_flags (compile_t *comptest, chararr_t *flags)
{
  bool        cpreprocess = false;
  const char  *p;
  const char  **flagarr;
  int         count = 0;

  if (flags == NULL) {
    return false;
  }

  flagarr = chararr_get_arr (flags);
  if (flagarr == NULL) {
    return false;
  }

  while ((p = flagarr [count++]) != NULL) {
    if (strcmp (p, "-E") == 0) {
      cpreprocess = true;
    }
    chararr_append (comptest->targv, p);
  }

  return cpreprocess;
}
