/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>

#include "mkc_ast.h"
#include "mkc_check.h"
#include "mkc_def.h"
#include "mkc_error.h"
#include "mkc_fileop.h"
#include "mkc_log.h"
#include "mkc_process.h"
#include "mkc_profile.h"
#include "mkc_pvar.h"
#include "mkc_string.h"
#include "mkc_util.h"

typedef struct mkc_process_t {
  mkc_profile_t         * profiles;
  mkc_pvar_t            * pvar;
  mkc_check_t           * check;
  const char            * compiler;
  const char            * sfx;
  const char            * testcc;
  const char            * testsfx;
  char                  * currentname;
  mkc_error_t           * mkcerr;
  mkc_log_t             * log;
  mkc_system_type_t     systype;
  mkc_system_id_t       sysid;
  mkc_compiler_id_t     compid;
  mkc_lib_loc_t         libloc;
  mkc_header_t          headertype;
  bool                  variadicmacro;
  bool                  negate;
  bool                  cacheloaded;
  bool                  cacheinvalidated;
  bool                  fromcache;
} mkc_process_t;

static const char *sysnames [MKC_SYS_MAX] = {
  [MKC_SYS_AIX] = "MKC_SYS_AIX",
  [MKC_SYS_ANDROID] = "MKC_SYS_ANDROID",
  [MKC_SYS_BSD] = "MKC_SYS_BSD",
  [MKC_SYS_LINUX] = "MKC_SYS_LINUX",
  [MKC_SYS_MACOS] = "MKC_SYS_MACOS",
  [MKC_SYS_SOLARIS] = "MKC_SYS_SOLARIS",
  [MKC_SYS_UNKNOWN] = "MKC_SYS_UNKNOWN",
  [MKC_SYS_WINDOWS] = "MKC_SYS_WINDOWS",
};

static const char *sysidnames [MKC_SYS_ID_MAX] = {
  [MKC_SYS_ID_CYGWIN] = "MKC_SYS_ID_CYGWIN",
  [MKC_SYS_ID_DRAGONFLYBSD] = "MKC_SYS_ID_DRAGONFLYBSD",
  [MKC_SYS_ID_FREEBSD] = "MKC_SYS_ID_FREEBSD",
  [MKC_SYS_ID_MSYS2] = "MKC_SYS_ID_MSYS2",
  [MKC_SYS_ID_NETBSD] = "MKC_SYS_ID_NETBSD",
  [MKC_SYS_ID_NOTSET] = "MKC_SYS_ID_NOTSET",
  [MKC_SYS_ID_OPENBSD] = "MKC_SYS_ID_OPENBSD",
};

static const char *compidnames [MKC_COMP_ID_MAX] = {
  [MKC_COMP_ID_CLANG] = "MKC_COMPILER_ID_CLANG",
  [MKC_COMP_ID_GCC] = "MKC_COMPILER_ID_GCC",
  [MKC_COMP_ID_ICC] = "MKC_COMPILER_ID_ICC",
  [MKC_COMP_ID_MSC] = "MKC_COMPILER_ID_MSC",
  [MKC_COMP_ID_SOLARIS] = "MKC_COMPILER_ID_SOLARIS",
  [MKC_COMP_ID_UNKNOWN] = "MKC_COMPILER_ID_UNKNOWN",
  [MKC_COMP_ID_XLC] = "MKC_COMPILER_ID_XLC",
};

enum {
  MKC_NO_VARIADIC_MACRO = false,
  MKC_VARIADIC_MACRO_SUPPORTED = true,
};

static const char * const liblocname = "MKC_LIB_LOC_LIB64";
static const char * const shlibext = "MKC_SHARED_LIBRARY_EXTENSION";
static const char * const objext = "MKC_OBJECT_EXTENSION";
static const char * const exeext = "MKC_EXECUTABLE_EXTENSION";
static const char * const mkctesthdrlist = "MKC_TV_TEST_HEADER_LIST";
static const char * const mkcwhilelimit = "MKC_TV_WHILE_LIMIT";
static const char * const mkctempvarpfx = "MKC_TV_";
static size_t mkctvpfxlen = 0;
static const char * const mkcisystype = "MKC_I_SYSTYPE";
static const char * const mkcisysid = "MKC_I_SYSID";
static const char * const mkcicompid= "MKC_I_COMPID";

static void mkc_process_var_print (mkc_process_t *process, const char *pname);
static void mkc_process_prof_print (mkc_process_t *process);
const char * mkc_process_create_name (mkc_process_t *process, char *buff, size_t sz, const char *tag, ...);
static int mkc_process_int_checks (mkc_process_t *process);
static void mkc_process_set_defaults (mkc_process_t *process);

mkc_process_t *
mkc_process_init (mkc_profile_t *profiles, mkc_log_t *log, mkc_error_t *mkcerr)
{
  mkc_process_t     *process;
  mkc_profidx_t     pidx;
  int               rc;

  mkctvpfxlen = strlen (mkctempvarpfx);
  process = malloc (sizeof (mkc_process_t));

  process->profiles = profiles;
  process->compiler = NULL;
  process->check = NULL;
  process->pvar = NULL;
  process->currentname = NULL;
  process->negate = false;
  process->cacheloaded = false;
  process->cacheinvalidated = false;
  process->fromcache = false;
  process->mkcerr = mkcerr;
  process->log = log;

  process->systype = MKC_SYS_UNKNOWN;
  process->sysid = MKC_SYS_ID_NOTSET;
  process->compid = MKC_COMP_ID_UNKNOWN;
  process->libloc = MKC_LIB_LOC_NOTSET;
  process->headertype = MKC_HEADER_MODERN;
  process->variadicmacro = MKC_VARIADIC_MACRO_SUPPORTED;

  process->pvar = mkc_pvar_init (process->profiles, log, mkcerr);
  if (process->pvar == NULL) {
    mkc_process_free (process);
    return NULL;
  }

  process->check = mkc_check_init (process->profiles, process->pvar, log, mkcerr);
  if (process->check == NULL) {
    mkc_process_free (process);
    return NULL;
  }

  mkc_profile_push (process->profiles);

  mkc_process_set_defaults (process);
  rc = mkc_process_int_checks (process);
  if (rc < 0) {
    mkc_process_free (process);
    return NULL;
  }

  pidx = mkc_profile_pop (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, pidx);
  mkc_pvar_set_fromcache (process->pvar, process->fromcache);

  return process;
}

void
mkc_process_free (mkc_process_t *process)
{
  if (process == NULL) {
    return;
  }

  if (process->pvar != NULL) {
    mkc_pvar_free (process->pvar);
  }
  if (process->check != NULL) {
    mkc_check_free (process->check);
  }
  free (process);
}

void
mkc_process_set_compiler (mkc_process_t *process,
    const char *compiler, const char *sfx)
{
  if (process == NULL || compiler == NULL) {
    return;
  }

  process->compiler = compiler;
  process->sfx = sfx;
}

void
mkc_process_set_fromcache (mkc_process_t *process, bool flag)
{
  if (process == NULL) {
    return;
  }

  if (! flag && process->cacheloaded && process->cacheinvalidated) {
    mkc_profidx_t   piter;
    mkc_profidx_t   tpidx;

    /* something changed that requires cache invalidation */
    mkc_profile_iter_start (process->profiles, &piter);
    while ((tpidx = mkc_profile_iter_next (process->profiles, &piter)) != MKC_ITER_FINISH) {
      if (mkc_pvar_profile_set_idx (process->pvar, tpidx) == MKC_PROF_NOT_FOUND) {
        continue;
      }
      mkc_profile_clear (process->profiles, tpidx);
    }

    mkc_message ("-- cache invalidated\n");
    mkc_log (process->log, MKC_LOG_GENERAL, "-- cache invalidated\n");
    mkc_process_set_defaults (process);
    mkc_process_int_checks (process);
  }

  if (flag) {
    process->cacheloaded = true;
  }
  process->fromcache = flag;
  mkc_pvar_set_fromcache (process->pvar, flag);
}

int32_t
mkc_process_condition (mkc_process_t *process, mkc_value_t *value)
{
  int32_t   rval;

  if (process == NULL) {
    return 0;
  }

  rval = mkc_pvar_value_get_integer (process->pvar, value);
  return rval;
}

int32_t
mkc_process_num_op (mkc_process_t *process, int type,
    mkc_value_t *vala, mkc_value_t *valb)
{
  int32_t   result = 0;
  int32_t   ivala, ivalb;
  char      tbuff [MKC_PATH_MAX];

  if (process == NULL) {
    return 0;
  }

  mkc_log (process->log, MKC_LOG_PROCESS, "  p-num-op-a: %s\n",
      mkc_value_to_str (vala, tbuff, sizeof (tbuff)));
  mkc_log (process->log, MKC_LOG_PROCESS, "  p-num-op-b: %s\n",
      mkc_value_to_str (valb, tbuff, sizeof (tbuff)));
  ivala = mkc_pvar_value_get_integer (process->pvar, vala);
  ivalb = mkc_pvar_value_get_integer (process->pvar, valb);
  mkc_log (process->log, MKC_LOG_PROCESS,
      "  p-num-op: %d %d\n", ivala, ivalb);

  switch (type) {
    case MKC_T_OP_NUM_EQ: {
      result = (ivala == ivalb);
      break;
    }
    case MKC_T_OP_NUM_NE: {
      result = (ivala != ivalb);
      break;
    }
    case MKC_T_OP_NUM_LT: {
      result = (ivala < ivalb);
      break;
    }
    case MKC_T_OP_NUM_LE: {
      result = (ivala <= ivalb);
      break;
    }
    case MKC_T_OP_NUM_GT: {
      result = (ivala > ivalb);
      break;
    }
    case MKC_T_OP_NUM_GE: {
      result = (ivala >= ivalb);
      break;
    }
    case MKC_T_OP_PLUS: {
      result = ivala + ivalb;
      break;
    }
    case MKC_T_OP_MINUS: {
      result = ivala - ivalb;
      break;
    }
    case MKC_T_OP_MULTIPLY: {
      result = ivala * ivalb;
      break;
    }
    case MKC_T_OP_DIVIDE: {
      if (ivalb == 0) {
        mkc_error_set (process->mkcerr, MKC_ERR_DIVIDE_BY_ZERO);
        break;
      }
      result = ivala / ivalb;
      break;
    }
    case MKC_T_OP_MODULO: {
      if (ivalb == 0) {
        mkc_error_set (process->mkcerr, MKC_ERR_DIVIDE_BY_ZERO);
        break;
      }
      result = ivala % ivalb;
      break;
    }
    default: {
      result = 0;
      mkc_error_set (process->mkcerr, MKC_ERR_INVALID_OP);
      break;
    }
  }

  return result;
}

int32_t
mkc_process_str_op (mkc_process_t *process, int type,
    mkc_value_t *vala, mkc_value_t *valb)
{
  int32_t     result = 0;
  char        stra [MKC_PATH_MAX];
  char        strb [MKC_PATH_MAX];

  if (process == NULL) {
    return 0;
  }

  mkc_log (process->log, MKC_LOG_PROCESS, "  p-str-op-a: %s\n",
      mkc_value_to_str (vala, stra, sizeof (stra)));
  mkc_log (process->log, MKC_LOG_PROCESS, "  p-str-op-b: %s\n",
      mkc_value_to_str (valb, strb, sizeof (strb)));
  mkc_pvar_value_get_str (process->pvar, vala, stra, sizeof (stra));
  mkc_pvar_value_get_str (process->pvar, valb, strb, sizeof (strb));
  mkc_log (process->log, MKC_LOG_PROCESS, "  p-str-op: |%s|%s|\n",
      stra, strb);

  switch (type) {
    case MKC_T_OP_STR_EQ: {
      result = strcmp (stra, strb) == 0;
      break;
    }
    case MKC_T_OP_STR_NE: {
      result = strcmp (stra, strb) != 0;
      break;
    }
    case MKC_T_OP_STR_LT: {
      result = strcmp (stra, strb) < 0;
      break;
    }
    case MKC_T_OP_STR_LE: {
      result = strcmp (stra, strb) <= 0;
      break;
    }
    case MKC_T_OP_STR_GT: {
      result = strcmp (stra, strb) > 0;
      break;
    }
    case MKC_T_OP_STR_GE: {
      result = strcmp (stra, strb) >= 0;
      break;
    }
    default: {
      result = 0;
      mkc_error_set (process->mkcerr, MKC_ERR_INVALID_OP);
      break;
    }
  }

  return result;
}

int32_t
mkc_process_unary_op (mkc_process_t *process, int type, mkc_value_t *vala)
{
  int32_t   result = 0;
  int32_t   ivala;

  if (process == NULL) {
    return 0;
  }

  ivala = mkc_pvar_value_get_integer (process->pvar, vala);

  switch (type) {
    case MKC_T_OP_NOT: {
      result = ! ivala;
      break;
    }
    case MKC_T_OP_UNARY_MINUS: {
      result = - ivala;
      break;
    }
    case MKC_T_OP_UNARY_PLUS: {
      result = ivala;
      break;
    }
    default: {
      result = 0;
      mkc_error_set (process->mkcerr, MKC_ERR_INVALID_OP);
      break;
    }
  }

  return result;
}

void
mkc_process_stmt_print (mkc_process_t *process, mkc_value_t *value, int depth)
{
  char      tbuff [MKC_PATH_MAX];

  if (process == NULL) {
    return;
  }
  if (value == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_INVALID_ARGUMENT);
    return;
  }

  mkc_pvar_value_get_str (process->pvar, value, tbuff, sizeof (tbuff));
  fprintf (stdout, "%s", tbuff);

  if (depth == 0) {
    fprintf (stdout, "\n");
    fflush (stdout);
  }
}

bool
mkc_process_profile_is_current (mkc_process_t *process, mkc_value_t *valnm)
{
  char              nm [MKC_VNAME_MAX];
  bool              rc = false;

  if (process == NULL) {
    return false;
  }

  mkc_pvar_value_get_str (process->pvar, valnm, nm, sizeof (nm));

  rc = mkc_profile_is_current (process->profiles, nm);
  return rc;
}

void
mkc_process_stmt_profile (mkc_process_t *process,
    mkc_value_t *valnm, mkc_value_t *valcomp)
{
  char              nm [MKC_VNAME_MAX];
  char              comp [MKC_VNAME_MAX];
  mkc_profidx_t     pidx;

  mkc_pvar_value_get_str (process->pvar, valnm, nm, sizeof (nm));

  *comp = '\0';
  if (valcomp != NULL) {
    mkc_pvar_value_get_str (process->pvar, valcomp, comp, sizeof (comp));
  }

  pidx = mkc_profile_find (process->profiles, nm, comp);
  if (pidx == MKC_PROF_NOT_FOUND) {
    pidx = mkc_profile_create (process->profiles, nm, comp, MKC_PROF_TYPE_USER);
  }

  mkc_pvar_profile_set_idx (process->pvar, pidx);
  mkc_pvar_set_fromcache (process->pvar, process->fromcache);
}

int
mkc_process_stmt_debug (mkc_process_t *process,
    mkc_value_t *value, mkc_value_t *subvalue)
{
  char    tbuff [MKC_VNAME_MAX];


  mkc_pvar_value_get_str (process->pvar, value, tbuff, sizeof (tbuff));

  if (strcmp (tbuff, "null") == 0) {
    /* do nothing */ ;
  }
  if (strcmp (tbuff, "vardebug") == 0) {
    mkc_pvar_debug (process->pvar);
  }
  if (strcmp (tbuff, "printprof") == 0) {
    mkc_process_prof_print (process);
  }
  if (strcmp (tbuff, "printvar") == 0) {
    mkc_pvar_value_get_str (process->pvar, subvalue, tbuff, sizeof (tbuff));
    mkc_process_var_print (process, tbuff);
  }

  return false;
}

void
mkc_process_stmt_set (mkc_process_t *process,
    mkc_value_t *valnm, mkc_value_t *value)
{
  char              nm [MKC_VNAME_MAX];
  char              buff [MKC_PATH_MAX];
  mkc_value_t       tvalue;
  mkc_value_t       *nvalue = NULL;
  mkc_err_code_t    trc;

  if (process == NULL) {
    return;
  }
  if (valnm == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_INVALID_ARGUMENT);
    return;
  }

  mkc_pvar_value_get_str (process->pvar, valnm, nm, sizeof (nm));

  if (*nm == '\0') {
    mkc_error_set (process->mkcerr, MKC_ERR_INVALID_ARGUMENT);
    return;
  }

  if (value->vtype == MKC_VT_ENV_VARIABLE ||
      value->vtype == MKC_VT_QUOTED_STRING) {
    /* need to get the actual value */
    mkc_pvar_value_get_str (process->pvar, value, buff, sizeof (buff));
    tvalue.sval = buff;
    tvalue.vtype = MKC_VT_STRING;
    value = &tvalue;
  }
  if (value->vtype == MKC_VT_VARIABLE) {
    char    *tstr;

    /* substitution must be run on the variable before the value is fetched */
    tstr = mkc_pvar_substitute (process->pvar, value->sval, 0);
    if (tstr != NULL) {
      nvalue = mkc_process_get_value (process, tstr);
      value = nvalue;
      free (tstr);
    }
  }

  trc = mkc_pvar_set (process->pvar, nm, value);
  if (trc == MKC_OK_CHANGE) {
    process->cacheinvalidated = true;
  }
}

void
mkc_process_attr_name (mkc_process_t *process, mkc_value_t *valnm)
{
  char    nm [MKC_VNAME_MAX];

  if (process == NULL) {
    return;
  }

  mkc_pvar_value_get_str (process->pvar, valnm, nm, sizeof (nm));
  if (process->currentname != NULL) {
    free (process->currentname);
  }
  process->currentname = strdup (nm);
  if (process->currentname == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_OUT_OF_MEMORY);
  }
}

void
mkc_process_attr_negate (mkc_process_t *process)
{
  if (process == NULL) {
    return;
  }

  process->negate = true;
}

void
mkc_process_attr_header (mkc_process_t *process, mkc_value_t *value)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;
  char            * hdrtxt = NULL;
  size_t          hdrtxtlen = 1;
  mkc_profidx_t   pidx;

  if (process == NULL) {
    return;
  }

  mkc_list_iter_start (value->list, &iteridx);
  while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    char        tbuff [MKC_PATH_MAX];
    mkc_value_t *lvalue;
    size_t      tlen;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    snprintf (tbuff, sizeof (tbuff), "#include <%s>\n", lvalue->sval);
    tlen = strlen (tbuff);
    hdrtxtlen += tlen;
    hdrtxt = realloc (hdrtxt, hdrtxtlen);
    stpecpy (hdrtxt + hdrtxtlen - tlen - 1, hdrtxt + hdrtxtlen, tbuff);
  }

  mkc_profile_push (process->profiles);
  mkc_pvar_profile_set (process->pvar, MKC_PROF_INTERNAL_NAME,
      MKC_PROF_COMPILER_GENERAL);
  mkc_pvar_set_fromcache (process->pvar, process->fromcache);
  mkc_pvar_set_str (process->pvar, mkctesthdrlist, hdrtxt);

  pidx = mkc_profile_pop (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, pidx);
  mkc_pvar_set_fromcache (process->pvar, process->fromcache);

  return;
}

void
mkc_process_attr_comp_flags (mkc_process_t *process, mkc_value_t *value)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;

  if (process == NULL) {
    return;
  }

  mkc_list_iter_start (value->list, &iteridx);
  while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    char        tbuff [MKC_VNAME_MAX];
    mkc_value_t *lvalue;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    mkc_pvar_value_get_str (process->pvar, lvalue, tbuff, sizeof (tbuff));
    mkc_chk_append_comp_flag (process->check, tbuff);
  }

  return;
}

void
mkc_process_attr_link_flags (mkc_process_t *process, mkc_value_t *value)
{
  mkc_listidx_t   iteridx;
  mkc_listidx_t   lidx;

  if (process == NULL) {
    return;
  }

  mkc_list_iter_start (value->list, &iteridx);
  while ((lidx = mkc_list_iter_next (value->list, &iteridx)) != MKC_ITER_FINISH) {
    char        tbuff [MKC_VNAME_MAX];
    mkc_value_t *lvalue;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    lvalue = mkc_list_get_by_idx (value->list, lidx);
    mkc_pvar_value_get_str (process->pvar, lvalue, tbuff, sizeof (tbuff));
    mkc_chk_append_link_flag (process->check, tbuff);
  }

  return;
}

mkc_value_t *
mkc_process_get_value (mkc_process_t *process, const char *nm)
{
  mkc_value_t   *value;

  value = mkc_pvar_get_by_profile (process->pvar, nm);
  return value;
}

int32_t
mkc_process_chk_compiler_flag (mkc_process_t *process,
    mkc_value_t *valflag, int addchk)
{
  int         rc = 0;
  char        tnm [MKC_VNAME_MAX];
  char        flag [MKC_VNAME_MAX];
  mkc_pvar_t  *pvar;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  mkc_process_set_compiler (process, process->testcc, process->testsfx);
  mkc_pvar_value_get_str (process->pvar, valflag, flag, sizeof (flag));
  mkc_process_create_name (process, tnm, sizeof (tnm), "cf_", flag, NULL);

  pvar = process->pvar;

  if (addchk == MKC_CHK) {
    rc = mkc_chk_compiler_flag (process->check,
        process->compiler, process->sfx, flag, process->negate);
  }
  process->negate = false;

  if (rc == 0) {
    const char  *tmp;

    tmp = mkc_pvar_name_alloc (pvar, tnm);
    mkc_pvar_set_str (pvar, tmp, flag);
  }

  if (addchk == MKC_ADD) {
    mkc_message ("-- add compiler flag: %s\n", flag);
    mkc_log (process->log, MKC_LOG_CHECK,
        "-- add compiler flag: %s\n", flag);
  }
  if (addchk == MKC_CHK) {
    mkc_message ("-- check compiler flag: %s - %s\n",
        flag, mkc_success_msg (rc));
    mkc_log (process->log, MKC_LOG_CHECK, "-- check compiler flag: %s - %s\n",
        flag, mkc_success_msg (rc));
  }

  return rc;
}


int32_t
mkc_process_chk_link_flag (mkc_process_t *process,
    mkc_value_t *valflag, int addchk)
{
  int         rc = 0;
  char        tnm [MKC_VNAME_MAX];
  char        flag [MKC_VNAME_MAX];
  mkc_pvar_t  *pvar;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  pvar = process->pvar;
  mkc_process_set_compiler (process, process->testcc, process->testsfx);
  mkc_pvar_value_get_str (process->pvar, valflag, flag, sizeof (flag));
  mkc_process_create_name (process, tnm, sizeof (tnm), "lf_", flag, NULL);

  if (addchk == MKC_CHK) {
    rc = mkc_chk_link_flag (process->check,
        process->compiler, process->sfx, flag);
  }
  if (rc == 0) {
    const char  *tmp;

    tmp = mkc_pvar_name_alloc (pvar, tnm);
    mkc_pvar_set_str (pvar, tmp, flag);
  }

  if (addchk == MKC_ADD) {
    mkc_message ("-- add link flag: %s\n", flag);
    mkc_log (process->log, MKC_LOG_CHECK,
        "-- add link flag: %s\n", flag);
  }
  if (addchk == MKC_CHK) {
    mkc_message ("-- check link flag: %s - %s\n",
        flag, mkc_success_msg (rc));
    mkc_log (process->log, MKC_LOG_CHECK, "-- check link flag: %s - %s\n",
        flag, mkc_success_msg (rc));
  }

  return rc;
}

int32_t
mkc_process_chk_size (mkc_process_t *process, mkc_value_t *valtype)
{
  int         rc;
  char        tnm [MKC_VNAME_MAX];
  char        type [MKC_VNAME_MAX];
  mkc_pvar_t  *pvar;
  const char  *tmp;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  pvar = process->pvar;
// ### for the time being, need to figure this out...
  mkc_process_set_compiler (process, process->testcc, process->testsfx);
  mkc_pvar_value_get_str (process->pvar, valtype, type, sizeof (type));
  mkc_process_create_name (process, tnm, sizeof (tnm), "_size_", type, NULL);

  rc = mkc_chk_size (process->check,
      process->compiler, process->sfx, type);
  tmp = mkc_pvar_name_alloc (pvar, tnm);
  mkc_pvar_set_integer (pvar, tmp, rc);

  mkc_message ("-- check size: %s : %d\n", type, rc);
  mkc_log (process->log, MKC_LOG_CHECK,
      "-- check size: %s : %d\n", type, rc);

  return rc;
}

int32_t
mkc_process_chk_type (mkc_process_t *process, mkc_value_t *valtype)
{
  int         rc;
  char        tnm [MKC_VNAME_MAX];
  char        type [MKC_VNAME_MAX];
  mkc_pvar_t  *pvar;
  const char  *tmp;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  pvar = process->pvar;
// ### for the time being, need to figure this out...
  mkc_process_set_compiler (process, process->testcc, process->testsfx);
  mkc_pvar_value_get_str (process->pvar, valtype, type, sizeof (type));
  mkc_process_create_name (process, tnm, sizeof (tnm), "_type_", type, NULL);

  rc = mkc_chk_type (process->check,
      process->compiler, process->sfx, type);
  tmp = mkc_pvar_name_alloc (pvar, tnm);
  mkc_pvar_set_integer (pvar, tmp, rc == 0 ? true : false);

  mkc_message ("-- check type: %s - %s\n",
      type, mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check type: %s - %s\n",
      type, mkc_success_msg (rc));

  return rc;
}

int32_t
mkc_process_chk_struct_member (mkc_process_t *process,
    mkc_value_t *valstructnm, mkc_value_t *valmembernm)
{
  int         rc;
  char        tnm [MKC_VNAME_MAX];
  char        structname [MKC_VNAME_MAX];
  char        membername [MKC_VNAME_MAX];
  mkc_pvar_t  *pvar;
  const char  *tmp;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }


  pvar = process->pvar;
// ### for the time being, need to figure this out...
  mkc_process_set_compiler (process, process->testcc, process->testsfx);
  mkc_pvar_value_get_str (process->pvar, valstructnm, structname, sizeof (structname));
  mkc_pvar_value_get_str (process->pvar, valmembernm, membername, sizeof (membername));
  mkc_process_create_name (process, tnm, sizeof (tnm),
      "_member_", structname, membername, NULL);

  rc = mkc_chk_struct_member (process->check,
      process->compiler, process->sfx, structname, membername);
  tmp = mkc_pvar_name_alloc (pvar, tnm);
  mkc_pvar_set_integer (pvar, tmp, rc == 0 ? true : false);

  mkc_message ("-- check struct member: %s.%s - %s\n",
      structname, membername, mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check struct member: %s.%s - %s\n",
      structname, membername, mkc_success_msg (rc));

  return rc;
}

int32_t
mkc_process_chk_function (mkc_process_t *process, mkc_value_t *valfuncnm)
{
  int         rc;
  char        tnm [MKC_VNAME_MAX];
  char        funcname [MKC_VNAME_MAX];
  mkc_pvar_t  *pvar;
  const char  *tmp;

  if (process == NULL) {
    return MKC_ERR_FAILURE;
  }

  pvar = process->pvar;
// ### for the time being, need to figure this out...
  mkc_process_set_compiler (process, process->testcc, process->testsfx);
  mkc_pvar_value_get_str (process->pvar, valfuncnm, funcname, sizeof (funcname));
  mkc_process_create_name (process, tnm, sizeof (tnm),
      "_function_", funcname, NULL);

  rc = mkc_chk_function (process->check,
      process->compiler, process->sfx, funcname);
  tmp = mkc_pvar_name_alloc (pvar, tnm);
  mkc_pvar_set_integer (pvar, tmp, rc == 0 ? true : false);

  mkc_message ("-- check function: %s - %s\n",
      funcname, mkc_success_msg (rc));
  mkc_log (process->log, MKC_LOG_CHECK, "-- check function: %s - %s\n",
      funcname, mkc_success_msg (rc));

  return rc;
}


void
mkc_process_local_set (mkc_process_t *process, const char *nm,
    const char *sval, mkc_profidx_t pidx)
{
  mkc_profidx_t   opidx;

  if (process == NULL) {
    return;
  }
  if (nm == NULL) {
    mkc_error_set (process->mkcerr, MKC_ERR_INVALID_ARGUMENT);
    return;
  }

  mkc_profile_push (process->profiles);
  mkc_profile_local_reset (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, pidx);
  mkc_pvar_set_str (process->pvar, nm, sval);
  opidx = mkc_profile_pop (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, opidx);
  mkc_pvar_set_fromcache (process->pvar, process->fromcache);
}

int32_t
mkc_process_get_while_limit (mkc_process_t *process)
{
  int32_t     limit = 10000;
  mkc_value_t *value;

  if (process == NULL) {
    return limit;
  }

  value = mkc_pvar_get_by_profile (process->pvar, mkcwhilelimit);
  if (value != NULL) {
    limit = mkc_pvar_value_get_integer (process->pvar, value);
  }

  return limit;
}

void
mkc_process_save_cache (mkc_process_t *process)
{
  mkc_profidx_t   piter;
  mkc_profile_t   *profiles;
  mkc_profidx_t   pidx;
  FILE            *fh;

  fh = mkc_fopen ("mkc_files/cache.mkc", "w");
  if (fh == NULL) {
    return;
  }
  mkc_profile_push (process->profiles);

  profiles = process->profiles;
  mkc_profile_iter_start (profiles, &piter);
  while ((pidx = mkc_profile_iter_next (profiles, &piter)) != MKC_ITER_FINISH) {
    const char      *nm;
    const char      *compname;
    mkc_varidx_t    viter;
    mkc_varidx_t    vidx;
    mkc_pvar_t      *pvar;
    int             count = 0;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    nm = mkc_profile_get_name (profiles, pidx);
    compname = mkc_profile_get_comp_name (profiles, pidx);
    fprintf (fh, "profile %s %s {\n", nm, compname);

    pvar = process->pvar;
    mkc_profile_push (process->profiles);

    if (mkc_pvar_profile_set_idx (process->pvar, pidx) == MKC_PROF_NOT_FOUND) {
      continue;
    }

    mkc_pvar_iter_start (pvar, &viter);
    while ((vidx = mkc_pvar_iter_next (pvar, &viter)) != MKC_ITER_FINISH) {
      const char      *nm;
      mkc_value_t     *value;

      nm = mkc_pvar_get_name (pvar, vidx);

      /* temporary variables do not need to be cached */
      if (strncmp (nm, mkctempvarpfx, mkctvpfxlen) == 0) {
        continue;
      }

      value = mkc_pvar_get_by_idx (pvar, vidx);

      if (value->vtype == MKC_VT_LIST) {
        mkc_list_t    *list;
        mkc_listidx_t lidx;
        mkc_listidx_t iteridx;

        list = value->list;
        mkc_list_iter_start (list, &iteridx);
        fprintf (fh, "  set %s [ ", nm);
        while ((lidx = mkc_list_iter_next (list, &iteridx)) != MKC_ITER_FINISH) {
          mkc_value_t   *tvalue;

          if (mkc_error_chk_err (process->mkcerr)) {
            break;
          }
          tvalue = mkc_list_get_by_idx (list, lidx);
          if (tvalue->vtype == MKC_VT_INTEGER) {
            fprintf (fh, "%d ", tvalue->ival);
          }
          if (tvalue->vtype != MKC_VT_INVALID &&
              tvalue->vtype != MKC_VT_INTEGER &&
              tvalue->vtype != MKC_VT_LIST) {
            fprintf (fh, "'%s' ", tvalue->sval);
          }
          if (tvalue->vtype == MKC_VT_LIST) {
// ### argh, will this be valid?
          }
        }
        fprintf (fh, "];\n");
        ++count;
      }
      if (value->vtype == MKC_VT_STRING) {
        const char  *val;

        val = value->sval;
        fprintf (fh, "  set %s '%s';\n", nm, val);
        ++count;
      }
      if (value->vtype == MKC_VT_INTEGER) {
        int32_t     ival;

        ival = value->ival;
        fprintf (fh, "  set %s %d;\n", nm, ival);
        ++count;
      }
    }

    pidx = mkc_profile_pop (process->profiles);
    mkc_pvar_profile_set_idx (process->pvar, pidx);
    mkc_pvar_set_fromcache (process->pvar, process->fromcache);

    if (count == 0) {
      fprintf (fh, "  ;\n");
    }
    fprintf (fh, "}\n");
  }

  pidx = mkc_profile_pop (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, pidx);
  mkc_pvar_set_fromcache (process->pvar, process->fromcache);

  fclose (fh);
}

/* internal routines */

static void
mkc_process_var_print (mkc_process_t *process, const char *pname)
{
  mkc_varidx_t    iteridx;
  mkc_varidx_t    vidx;
  mkc_pvar_t      *pvar;
  bool            intest = false;
  bool            ininternal = false;
  mkc_profidx_t   pidx;

  pvar = process->pvar;
  mkc_profile_push (process->profiles);
  if (pname != NULL && strcmp (pname, "test") == 0) {
    pname = MKC_PROF_GLOBAL_NAME;
    intest = true;
  }
  if (pname != NULL && strcmp (pname, "internalall") == 0) {
    pname = MKC_PROF_INTERNAL_NAME;
  }
  if (pname != NULL && strcmp (pname, "internal") == 0) {
    ininternal = true;
  }

  for (mkc_prof_comp_t i = 0; i < MKC_PROF_COMPILER_MAX; ++i) {
    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    if (mkc_pvar_profile_set (process->pvar, pname, i) == MKC_PROF_NOT_FOUND) {
      continue;
    }
    fprintf (stdout, "== %s %d\n", pname, i);

    mkc_pvar_iter_start (pvar, &iteridx);

    while ((vidx = mkc_pvar_iter_next (pvar, &iteridx)) != MKC_ITER_FINISH) {
      const char      *nm;
      mkc_value_t     *value;

      nm = mkc_pvar_get_name (pvar, vidx);
      value = mkc_pvar_get_by_idx (pvar, vidx);

      if (ininternal) {
        /* the variables that are used internally are */
        /* generally not interesting */
        if (strncmp (nm, mkctempvarpfx, mkctvpfxlen) == 0) {
          continue;
        }
      }

      if (intest) {
        if (strcmp (nm, "CC") == 0 ||
            strcmp (nm, "CXX") == 0 ||
            strcmp (nm, "OBJC") == 0 ||
            strcmp (nm, "BISON") == 0 ||
            strcmp (nm, "FLEX") == 0 ||
            strcmp (nm, "_header_modern") == 0 ||
            strcmp (nm, "_has_variadic_macro") == 0) {
          continue;
        }
      }

      if (value->vtype == MKC_VT_LIST) {
        mkc_list_t    *list;
        mkc_listidx_t lidx;
        mkc_listidx_t iteridx;

        list = value->list;
        mkc_list_iter_start (list, &iteridx);
        fprintf (stdout, "  %s [ ", nm);
        while ((lidx = mkc_list_iter_next (list, &iteridx)) != MKC_ITER_FINISH) {
          mkc_value_t   *tvalue;

          if (mkc_error_chk_err (process->mkcerr)) {
            break;
          }
          tvalue = mkc_list_get_by_idx (list, lidx);
          if (tvalue->vtype == MKC_VT_INTEGER) {
            fprintf (stdout, "%d ", tvalue->ival);
          }
          if (tvalue->vtype != MKC_VT_INVALID &&
              tvalue->vtype != MKC_VT_INTEGER &&
              tvalue->vtype != MKC_VT_LIST) {
            fprintf (stdout, "%s ", tvalue->sval);
          }
          if (tvalue->vtype == MKC_VT_LIST) {
            fprintf (stdout, "[...] ");
          }
        }
        fprintf (stdout, "]\n");
      }
      if (value->vtype == MKC_VT_STRING) {
        const char  *val;

        val = value->sval;
        fprintf (stdout, "  %s %s\n", nm, val);
      }
      if (value->vtype == MKC_VT_INTEGER) {
        int32_t     ival;

        ival = value->ival;
        fprintf (stdout, "  %s %d\n", nm, ival);
      }
    }
  }

  pidx = mkc_profile_pop (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, pidx);
  mkc_pvar_set_fromcache (process->pvar, process->fromcache);
}

static void
mkc_process_prof_print (mkc_process_t *process)
{
  mkc_varidx_t   iteridx;
  mkc_varidx_t   idx;
  mkc_profile_t   *profiles;

  fprintf (stdout, "== profiles\n");

  profiles = process->profiles;
  mkc_profile_iter_start (profiles, &iteridx);
  while ((idx = mkc_profile_iter_next (profiles, &iteridx)) != MKC_ITER_FINISH) {
    const char      *nm;
    mkc_prof_comp_t compiler;

    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }

    nm = mkc_profile_get_name (profiles, idx);
    compiler = mkc_profile_get_compiler (profiles, idx);
    fprintf (stdout, "  %s %d\n", nm, compiler);
  }
}

const char *
mkc_process_create_name (mkc_process_t *process, char *buff, size_t sz,
    const char *tag, ...)
{
  char        *p;
  size_t      len;
  size_t      nlen = 0;
  const char  * str;
  va_list     ap;

  va_start (ap, tag);

  if (process->currentname != NULL) {
    stpecpy (buff, buff + sz, process->currentname);
    free (process->currentname);
    process->currentname = NULL;
    va_end (ap);
    return buff;
  }

  p = stpecpy (buff, buff + sz, tag);

  /* the caller must pass in a NULL terminal indicator */
  while ((str = va_arg (ap, const char *)) != NULL) {
    size_t      start = 0;

    len = strlen (str);
    nlen = strlen (buff);

    if (nlen > 0 && *(buff + nlen - 1) != '_') {
      p = stpecpy (p, buff + sz, "_");
      nlen += 1;
    }

    for (size_t i = start; i < len && i < sz - 1; ++i) {
      char    c;

      c = str [i];
      if (c == '\0') {
        break;
      }

      if (c == '-' && i == 0) {
        continue;
      }
      if (c == 'W' && i == 1) {
        continue;
      }

      if (! isalnum (c) && c != '_') {
        c = '_';
      }
      *p = c;
      ++nlen;
      ++p;
    }
    *p = '\0';
  }

  if (nlen >= 2) {
    if (strcmp (buff + nlen - 2, ".h") == 0 ||
        strcmp (buff + nlen - 2, ".c") == 0 ||
        strcmp (buff + nlen - 2, ".m") == 0 ||
        strcmp (buff + nlen - 2, ".l") == 0 ||
        strcmp (buff + nlen - 2, ".y") == 0) {
      buff [nlen - 2] = '\0';
    }
    nlen -= 2;
  }
   if (nlen >= 4) {
    if (strcmp (buff + nlen - 4, ".cpp") == 0 ||
        strcmp (buff + nlen - 4, ".hpp") == 0) {
      buff [nlen - 4] = '\0';
    }
    nlen -= 4;
  }

  return buff;
}

static int
mkc_process_int_checks (mkc_process_t *process)
{
  int                 rc;
  const char          *testcc;
  mkc_compiler_t      tcompiler;
  mkc_profidx_t       pidx;
  mkc_value_t         *value;
  mstime_t            starttm;

  mstimestart (&starttm);
  mkc_create_dirs ();

  mkc_profile_push (process->profiles);
  mkc_log (process->log, MKC_LOG_CHECK, "== internal checks\n");

  mkc_pvar_profile_set (process->pvar, MKC_PROF_GLOBAL_NAME, MKC_PROF_COMPILER_GENERAL);
  rc = mkc_chk_compiler_env (process->check);
  if (rc == MKC_OK_CHANGE) {
    return MKC_OK_CHANGE;
  }

  value = mkc_pvar_get_value (process->pvar, "CC");
  process->testcc = value->sval;
  testcc = value->sval;

  mkc_pvar_profile_set (process->pvar, MKC_PROF_INTERNAL_NAME, MKC_PROF_COMPILER_GENERAL);

  process->testsfx = mkc_chk_guess_suffix (process->testcc);

  rc = mkc_chk_compiler_works (process->check, testcc, process->testsfx);
  if (rc != 0) {
    mkc_error_set (process->mkcerr, MKC_ERR_COMPILER_FAILURE);
    return MKC_ERR_FAILURE;
  }

  tcompiler = MKC_COMP_UNKNOWN;
  rc = mkc_chk_which_compiler (process->check, testcc, process->testsfx);
  if (rc >= 0) {
    tcompiler = rc;
  }

  process->testsfx = mkc_chk_compiler_suffix (tcompiler);

  rc = mkc_chk_compiler_id (process->check, testcc, process->testsfx);
  if (rc >= 0) {
    process->compid = rc;
  }

  value = mkc_pvar_get_value (process->pvar, mkcicompid);
  if (value != NULL) {
    mkc_compiler_id_t   tcompid;

    tcompid = mkc_pvar_value_get_integer (process->pvar, value);
    if (process->compid != tcompid) {
      return MKC_OK_CHANGE;
    }
  }

  mkc_pvar_profile_set (process->pvar, MKC_PROF_GLOBAL_NAME, MKC_PROF_COMPILER_C);

  rc = mkc_chk_header_modern (process->check, testcc, process->testsfx);
  if (rc != 0) {
    process->headertype = MKC_HEADER_LEGACY;
  }
  mkc_pvar_set_integer (process->pvar, "_header_modern", process->headertype);

  mkc_pvar_profile_set (process->pvar, MKC_PROF_INTERNAL_NAME, MKC_PROF_COMPILER_GENERAL);

  rc = mkc_chk_system_type (process->check, testcc, process->testsfx);
  if (rc >= 0) {
    process->systype = rc;
  }

  value = mkc_pvar_get_value (process->pvar, mkcisystype);
  if (value != NULL) {
    mkc_system_type_t   tsystype;

    tsystype = mkc_pvar_value_get_integer (process->pvar, value);
    if (process->systype != tsystype) {
      return MKC_OK_CHANGE;
    }
  }

  /* object, executable extension */
  mkc_pvar_set_str (process->pvar, objext, ".o");
  mkc_pvar_set_str (process->pvar, exeext, "");
  if (process->systype == MKC_SYS_WINDOWS) {
    mkc_pvar_set_str (process->pvar, objext, ".obj");
    mkc_pvar_set_str (process->pvar, exeext, ".exe");
  }

  /* shared library extension */
  switch (process->systype) {
    case MKC_SYS_AIX: {
      mkc_pvar_set_str (process->pvar, shlibext, ".a");
      break;
    }
    case MKC_SYS_ANDROID:
    case MKC_SYS_BSD:
    case MKC_SYS_LINUX:
    case MKC_SYS_SOLARIS: {
      mkc_pvar_set_str (process->pvar, shlibext, ".so");
      break;
    }
    case MKC_SYS_MACOS: {
      mkc_pvar_set_str (process->pvar, shlibext, ".dylib");
      break;
    }
    case MKC_SYS_WINDOWS: {
      mkc_pvar_set_str (process->pvar, shlibext, ".dll");
      break;
    }
    default: {
      mkc_pvar_set_str (process->pvar, shlibext, ".so");
      break;
    }
  }

  rc = mkc_chk_system_id (process->check, testcc, process->testsfx);
  if (rc >= 0) {
    process->sysid = rc;
  }

  value = mkc_pvar_get_value (process->pvar, mkcisysid);
  if (value != NULL) {
    mkc_system_id_t     tsysid;

    tsysid = mkc_pvar_value_get_integer (process->pvar, value);
    if (process->sysid != tsysid) {
      return MKC_OK_CHANGE;
    }
  }

  mkc_pvar_profile_set (process->pvar, MKC_PROF_GLOBAL_NAME, MKC_PROF_COMPILER_C);

  rc = mkc_chk_variadic_macro (process->check, testcc, process->testsfx);
  if (rc != 0) {
    process->variadicmacro = MKC_NO_VARIADIC_MACRO;
  }
  mkc_pvar_set_integer (process->pvar, "_has_variadic_macro", process->variadicmacro);

  mkc_pvar_profile_set (process->pvar, MKC_PROF_INTERNAL_NAME, MKC_PROF_COMPILER_GENERAL);

  if (process->systype == MKC_SYS_LINUX) {
    rc = mkc_chk_library_location (process->check, testcc, process->testsfx);
    if (rc >= 0) {
      process->libloc = rc;
    }
  }

  mkc_pvar_set_integer (process->pvar, mkcisystype, process->systype);
  for (mkc_system_type_t i = 0; i < MKC_SYS_MAX; ++i) {
    if (process->systype == i) {
      mkc_pvar_set_integer (process->pvar, sysnames [i], true);
      break;
    }
  }

  mkc_pvar_set_integer (process->pvar, mkcisysid, process->sysid);
  for (mkc_system_id_t i = 0; i < MKC_SYS_ID_MAX; ++i) {
    if (process->sysid == i) {
      mkc_pvar_set_integer (process->pvar, sysidnames [i], true);
      break;
    }
  }

  mkc_pvar_set_integer (process->pvar, mkcicompid, process->compid);
  for (mkc_compiler_id_t i = 0; i < MKC_COMP_ID_MAX; ++i) {
    if (process->compid == i) {
      mkc_pvar_set_integer (process->pvar, compidnames [i], true);
      break;
    }
  }

  mkc_pvar_set_integer (process->pvar, liblocname, process->libloc);

  pidx = mkc_profile_pop (process->profiles);
  mkc_pvar_profile_set_idx (process->pvar, pidx);
  mkc_pvar_set_fromcache (process->pvar, process->fromcache);

  {
    char    tbuff [40];
    time_t  etm;

    etm = mstimeend (&starttm);
    mkc_elapsed_disp (etm, tbuff, sizeof (tbuff));
    mkc_message ("-- mkc internal setup: %s\n", tbuff);
    mkc_log (process->log, MKC_LOG_STATISTICS,
        "-- mkc internal setup: %s\n", tbuff);
  }

  return MKC_OK;
}

static void
mkc_process_set_defaults (mkc_process_t *process)
{
  /* create internal constants */
  mkc_pvar_profile_set (process->pvar,
      MKC_PROF_INTERNAL_NAME, MKC_PROF_COMPILER_GENERAL);

  for (mkc_system_type_t i = 0; i < MKC_SYS_MAX; ++i) {
    mkc_pvar_set_integer (process->pvar, sysnames [i], false);
  }
  for (mkc_system_id_t i = 0; i < MKC_SYS_ID_MAX; ++i) {
    mkc_pvar_set_integer (process->pvar, sysidnames [i], false);
  }

  mkc_pvar_set_integer (process->pvar, mkcwhilelimit, 10000);
  mkc_pvar_set_str (process->pvar, liblocname, "");

  mkc_pvar_profile_set (process->pvar, MKC_PROF_GLOBAL_NAME, MKC_PROF_COMPILER_GENERAL);
  mkc_pvar_set_str (process->pvar, "BISON", "bison");
  mkc_pvar_set_str (process->pvar, "CC", "cc");
  mkc_pvar_set_str (process->pvar, "CXX", "c++");
  mkc_pvar_set_str (process->pvar, "FLEX", "flex");
  mkc_pvar_set_str (process->pvar, "OBJC", "objc");

  mkc_pvar_profile_set (process->pvar, MKC_PROF_INTERNAL_NAME, MKC_PROF_COMPILER_GENERAL);
  for (mkc_compiler_id_t i = 0; i < MKC_COMP_ID_MAX; ++i) {
    if (mkc_error_chk_err (process->mkcerr)) {
      break;
    }
    mkc_pvar_set_integer (process->pvar, compidnames [i], false);
  }

  mkc_pvar_profile_set (process->pvar, MKC_PROF_GLOBAL_NAME, MKC_PROF_COMPILER_C);

  mkc_pvar_set_integer (process->pvar, "_header_modern", process->headertype);
}
