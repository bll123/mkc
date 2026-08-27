/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_COMPILER_H
#define INC_MKC_COMPILER_H

#include <stddef.h>

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

/* the compiler */
typedef enum {
  MKC_COMPILER_BISON,
  MKC_COMPILER_C,
  MKC_COMPILER_D,
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

typedef enum {
  MKC_COMP_FLAG_TYPE_DEFAULT,
  MKC_COMP_FLAG_TYPE_MAX,
} mkc_compiler_flag_type_t;

typedef enum {
  MKC_COMP_FLAG_COMPILE,
  MKC_COMP_FLAG_DEPS,
  MKC_COMP_FLAG_DEPS_USER,
  MKC_COMP_FLAG_INCLUDE,
  MKC_COMP_FLAG_LIB,
  MKC_COMP_FLAG_LIBPATH,
  MKC_COMP_FLAG_LINKPREFIX,
  MKC_COMP_FLAG_OUTPUT,
  MKC_COMP_FLAG_PREPROCESS,
  MKC_COMP_FLAG_WARN_NO_DEPRECATE,
  MKC_COMP_FLAG_WARN_PREFIX,
  MKC_COMP_FLAG_WARN_NEGATE,
  MKC_COMP_FLAG_MAX,
} mkc_compiler_flag_t;

const char * compiler_get_name (mkc_compiler_t comp);
const char * compiler_get_suffix (mkc_compiler_t comp);
const char * compiler_get_env_name (mkc_compiler_t comp);
mkc_compiler_t compiler_get_id (const char *compiler);

const char * compiler_get_flag (mkc_compiler_id_t compid, mkc_compiler_flag_t flag);
size_t compiler_get_flag_len (mkc_compiler_id_t compid, mkc_compiler_flag_t flag);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_COMPILER_H */
