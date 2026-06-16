/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#pragma once

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

/* the compiler */
typedef enum {
  MKC_COMPILER_BISON,
  MKC_COMPILER_C,
  MKC_COMPILER_CXX,
  MKC_COMPILER_FLEX,
  MKC_COMPILER_GENERAL,
  MKC_COMPILER_OBJC,
  MKC_COMPILER_UNKNOWN,
  MKC_COMPILER_MAX,
} mkc_compiler_t;

typedef enum {
  MKC_COMP_ID_CLANG,
  MKC_COMP_ID_GCC,
  MKC_COMP_ID_ICC,
  MKC_COMP_ID_MSC,
  MKC_COMP_ID_SOLARIS,
  MKC_COMP_ID_XLC,
  MKC_COMP_ID_UNKNOWN,
  MKC_COMP_ID_MAX,
} mkc_compiler_id_t;

const char * mkc_compiler_get_name (mkc_compiler_t comp);
const char * mkc_compiler_get_suffix (mkc_compiler_t comp);
mkc_compiler_t mkc_compiler_get (const char *compiler);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

