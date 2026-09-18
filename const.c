/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

char const * const MKC_C_PROF_NAME_DEFAULT = "default";
char const * const MKC_C_PROF_NAME_INTERNAL = "internal";

/* some of these get allocated many times, keep them short */
/* as these are only used internally in the cache, readability is */
/* not an issue */
char const * const MKC_C_BVAR_COMPFLAGS = "cf";
char const * const MKC_C_BVAR_DEPENDENCY = "dep";
char const * const MKC_C_BVAR_LIBS = "lib";
char const * const MKC_C_BVAR_LINKFLAGS = "lf";
char const * const MKC_C_BVAR_TIMESTAMP = "ts";
char const * const MKC_C_BVAR_TYPE = "typ";
char const * const MKC_C_VAR_BUILD_DATA = "MKC_BUILD_DATA";
char const * const MKC_C_VAR_BUILD_PATHS = "MKC_BUILD_PATHS";

/* these are duplicated in process.c */
char const * const MKC_C_PATH_GETCONF = "MKC_PATH_GETCONF";
char const * const MKC_C_PATH_PKGCONF = "MKC_PATH_PKGCONF";
char const * const MKC_C_PATH_PKGCONFIG = "MKC_PATH_PKG_CONFIG";

char const * const MKC_C_PROFILE_NAME = "MKC_PROFILE_NAME";
char const * const MKC_C_COMPFLAGS = "MKC_CFLAGS";
char const * const MKC_C_LINKFLAGS = "MKC_LDFLAGS";
char const * const MKC_C_LIBS = "MKC_LIBS";
char const * const MKC_C_SUPPORTS_MM = "MKC_COMPILER_SUPPORTS_MM";
char const * const MKC_C_MKC_CHANGED = "MKC_MKC_CHANGED";
char const * const MKC_C_PREFIX = "MKC_PREFIX";

int gmkcdebug = 0;
