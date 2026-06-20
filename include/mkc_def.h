/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_DEF_H
#define INC_MKC_DEF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

enum {
  MKC_ADD,
  MKC_CHK,
};

enum {
  MKC_VNAME_MAX = 256,
};

#define TEMP_PATH_MAX PATH_MAX
/* windows defines path_max as 260. maxpathlen as 260. */
/* macos defines path_max as 1024. this seems short */
#if PATH_MAX <= 1024
# undef TEMP_PATH_MAX
# define TEMP_PATH_MAX 4096
#endif
enum {
  MKC_PATH_MAX = TEMP_PATH_MAX,
};
#undef TEMP_PATH_MAX

/* the base system type */
typedef enum {
  MKC_SYS_AIX,
  MKC_SYS_ANDROID,
  MKC_SYS_BSD,
  MKC_SYS_IOS,
  MKC_SYS_LINUX,
  MKC_SYS_MACOS,
  MKC_SYS_SOLARIS,
  MKC_SYS_UNKNOWN,
  MKC_SYS_WINDOWS,
  MKC_SYS_MAX,
} mkc_system_type_t;

typedef enum {
  MKC_SYS_ID_CYGWIN,
  MKC_SYS_ID_DRAGONFLYBSD,
  MKC_SYS_ID_FREEBSD,
  MKC_SYS_ID_MSYS2,
  MKC_SYS_ID_NETBSD,
  MKC_SYS_ID_NOTSET,
  MKC_SYS_ID_OPENBSD,
  MKC_SYS_ID_SOLARIS,
  MKC_SYS_ID_MAX,
} mkc_system_id_t;

typedef enum {
  MKC_LIB_LOC_LIB,
  MKC_LIB_LOC_LIB64,
  MKC_LIB_LOC_NOTSET,
} mkc_lib_loc_t;

typedef enum {
  MKC_HEADER_LEGACY = false,
  MKC_HEADER_MODERN = true,
} mkc_header_t;

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_DEF_H */
