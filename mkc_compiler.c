/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "mkc_compiler.h"

/* these are internal names, not the actual names of the compilers */
static char const * const compnames [MKC_COMPILER_MAX] = {
  [MKC_COMPILER_BISON] = "bison",
  [MKC_COMPILER_C] = "c",
  [MKC_COMPILER_CXX] = "c++",
  [MKC_COMPILER_D] = "d",
  [MKC_COMPILER_FLEX] = "flex",
  [MKC_COMPILER_GENERAL] = "general",
  [MKC_COMPILER_OBJC] = "objc",
  [MKC_COMPILER_UNKNOWN] = "unknown",
};

static char const * const compsuffix [MKC_COMPILER_MAX] = {
  [MKC_COMPILER_BISON] = ".y",
  [MKC_COMPILER_C] = ".c",
  [MKC_COMPILER_CXX] = ".cpp",
  [MKC_COMPILER_D] = ".d",
  [MKC_COMPILER_FLEX] = ".l",
  [MKC_COMPILER_GENERAL] = ".c",
  [MKC_COMPILER_OBJC] = ".m",
  [MKC_COMPILER_UNKNOWN] = ".c",
};

static char const * const compenv [MKC_COMPILER_MAX] = {
  [MKC_COMPILER_BISON] = "BISON",
  [MKC_COMPILER_C] = "CC",
  [MKC_COMPILER_D] = "DC",
  [MKC_COMPILER_CXX] = "CXX",
  [MKC_COMPILER_FLEX] = "FLEX",
  [MKC_COMPILER_GENERAL] = "CC",
  [MKC_COMPILER_OBJC] = "CC",
  [MKC_COMPILER_UNKNOWN] = "CC",
};

typedef struct mkc_compflag_t {
  const char  * name;
  size_t      len;
} mkc_compflag_t;

static mkc_compflag_t compflags [MKC_COMP_FLAG_TYPE_MAX][MKC_COMP_FLAG_MAX] = {
  [MKC_COMP_FLAG_TYPE_DEFAULT] = {
      [MKC_COMP_FLAG_COMPILE]           = { "-c",   2 },
      [MKC_COMP_FLAG_DEPS]              = { "-M",   2 },
      [MKC_COMP_FLAG_DEPS_USER]         = { "-MM",  3 },
      [MKC_COMP_FLAG_INCLUDE]           = { "-I",   2 },
      [MKC_COMP_FLAG_LIB]               = { "-l",   2 },
      [MKC_COMP_FLAG_LIBPATH]           = { "-L",   2 },
      [MKC_COMP_FLAG_LINKPREFIX]        = { "-Wl,", 4 },
      [MKC_COMP_FLAG_OUTPUT]            = { "-o",   2 },
      [MKC_COMP_FLAG_PREPROCESS]        = { "-E",   2 },
      [MKC_COMP_FLAG_WARN_PREFIX]       = { "-W",   2 },
      [MKC_COMP_FLAG_WARN_NEGATE]       = { "-Wno-", 5 },
      [MKC_COMP_FLAG_WARN_NO_DEPRECATE] = { "-Wno-deprecated", 15 },
      },
};

const char *
compiler_get_name (mkc_compiler_t comp)
{
  if (comp < 0 || comp >= MKC_COMPILER_MAX) {
    return NULL;
  }

  return compnames [comp];
}

const char *
compiler_get_suffix (mkc_compiler_t comp)
{
  if (comp < 0 || comp >= MKC_COMPILER_MAX) {
    return NULL;
  }

  return compsuffix [comp];
}

const char *
compiler_get_env_name (mkc_compiler_t comp)
{
  if (comp < 0 || comp >= MKC_COMPILER_MAX) {
    return NULL;
  }

  return compenv [comp];
}

mkc_compiler_t
compiler_get_id (const char *compiler)
{
  mkc_compiler_t    cid = MKC_COMPILER_GENERAL;

  if (compiler == NULL) {
    return cid;
  }

  for (mkc_compiler_t i = 0; i < MKC_COMPILER_MAX; ++i) {
    if (strcmp (compiler, compiler_get_name (i)) == 0) {
      cid = i;
      return cid;
    }
  }

  /* check for other spelling/case variants */
  /* the default is MKC_COMPILER_GENERAL */
  if (strcmp (compiler, "C") == 0) {
    cid = MKC_COMPILER_C;
  }
  if (strcmp (compiler, "D") == 0) {
    cid = MKC_COMPILER_D;
  }
  if (strcmp (compiler, "C++") == 0 ||
      strcmp (compiler, "cpp") == 0 ||
      strcmp (compiler, "cxx") == 0) {
    cid = MKC_COMPILER_CXX;
  }
  if (strcmp (compiler, "OBJC") == 0) {
    cid = MKC_COMPILER_OBJC;
  }

  return cid;
}

const char *
compiler_get_flag (mkc_compiler_id_t compid, mkc_compiler_flag_t flag)
{
  mkc_compiler_flag_type_t  flagtype = MKC_COMP_FLAG_TYPE_DEFAULT;
  const char                * flagstr;

  switch (compid) {
    default: {
      flagtype = MKC_COMP_FLAG_TYPE_DEFAULT;
      break;
    }
  }

  flagstr = compflags [flagtype][flag].name;
  return flagstr;
}

size_t
compiler_get_flag_len (mkc_compiler_id_t compid, mkc_compiler_flag_t flag)
{
  mkc_compiler_flag_type_t  flagtype = MKC_COMP_FLAG_TYPE_DEFAULT;
  size_t                    len;

  switch (compid) {
    default: {
      flagtype = MKC_COMP_FLAG_TYPE_DEFAULT;
      break;
    }
  }

  len = compflags [flagtype][flag].len;
  return len;
}

