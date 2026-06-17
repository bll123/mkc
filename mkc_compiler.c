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
static const char * const compnames [MKC_COMPILER_MAX] = {
  [MKC_COMPILER_BISON] = "bison",
  [MKC_COMPILER_C] = "c",
  [MKC_COMPILER_CXX] = "c++",
  [MKC_COMPILER_FLEX] = "flex",
  [MKC_COMPILER_GENERAL] = "general",
  [MKC_COMPILER_OBJC] = "objc",
  [MKC_COMPILER_UNKNOWN] = "unknown",
};

static const char * const compsuffix [MKC_COMPILER_MAX] = {
  [MKC_COMPILER_BISON] = ".y",
  [MKC_COMPILER_C] = ".c",
  [MKC_COMPILER_CXX] = ".cpp",
  [MKC_COMPILER_FLEX] = ".l",
  [MKC_COMPILER_GENERAL] = ".c",
  [MKC_COMPILER_OBJC] = ".m",
  [MKC_COMPILER_UNKNOWN] = ".c",
};

static const char * const compenv [MKC_COMPILER_MAX] = {
  [MKC_COMPILER_BISON] = "BISON",
  [MKC_COMPILER_C] = "CC",
  [MKC_COMPILER_CXX] = "CXX",
  [MKC_COMPILER_FLEX] = "FLEX",
  [MKC_COMPILER_GENERAL] = "CC",
  [MKC_COMPILER_OBJC] = "CC",
  [MKC_COMPILER_UNKNOWN] = "CC",
};

const char *
mkc_compiler_get_name (mkc_compiler_t comp)
{
  if (comp < 0 || comp >= MKC_COMPILER_MAX) {
    return NULL;
  }

  return compnames [comp];
}

const char *
mkc_compiler_get_suffix (mkc_compiler_t comp)
{
  if (comp < 0 || comp >= MKC_COMPILER_MAX) {
    return NULL;
  }

  return compsuffix [comp];
}

const char *
mkc_compiler_get_env_name (mkc_compiler_t comp)
{
  if (comp < 0 || comp >= MKC_COMPILER_MAX) {
    return NULL;
  }

  return compenv [comp];
}

mkc_compiler_t
mkc_compiler_get (const char *compiler)
{
  mkc_compiler_t    cid = MKC_COMPILER_GENERAL;

  if (compiler == NULL) {
    return cid;
  }

  for (mkc_compiler_t i = 0; i < MKC_COMPILER_MAX; ++i) {
    if (strcmp (compiler, mkc_compiler_get_name (i)) == 0) {
      cid = i;
      return cid;
    }
  }

  /* check for other spelling/case variants */
  /* the default is MKC_COMPILER_GENERAL */
  if (strcmp (compiler, "C") == 0) {
    cid = MKC_COMPILER_C;
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
