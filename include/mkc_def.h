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
  MKC_SMALL_BUFF_SZ = 8 * 1024,
  MKC_LARGE_BUFF_SZ = 1024 * 1024,
  MKC_HUGE_BUFF_SZ = 8 * 1024 * 1024,
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
  MKC_ISYS_AIX,
  MKC_ISYS_ANDROID,
  MKC_ISYS_BSD,
  MKC_ISYS_IOS,
  MKC_ISYS_LINUX,
  MKC_ISYS_MACOS,
  MKC_ISYS_SOLARIS,
  MKC_ISYS_UNKNOWN,
  MKC_ISYS_WINDOWS,
  MKC_ISYS_MAX,
} mkc_system_type_t;

/* update the variable names in process.c */
typedef enum {
  MKC_ISYS_ID_ALPINE,          // linux
  MKC_ISYS_ID_ARCH,            // linux
  MKC_ISYS_ID_AZURE,           // linux
  MKC_ISYS_ID_CYGWIN,
  MKC_ISYS_ID_DEBIAN,          // linux
  MKC_ISYS_ID_DRAGONFLYBSD,
  MKC_ISYS_ID_FEDORA,          // linux
  MKC_ISYS_ID_FREEBSD,
  MKC_ISYS_ID_GENTOO,          // linux
  MKC_ISYS_ID_MSYS2,
  MKC_ISYS_ID_NETBSD,
  MKC_ISYS_ID_NIXOS,           // linux
  MKC_ISYS_ID_NOTSET,
  MKC_ISYS_ID_OPENBSD,
  MKC_ISYS_ID_SLACKWARE,       // linux
  MKC_ISYS_ID_SOLARIS,
  MKC_ISYS_ID_SUSE,            // linux
  MKC_ISYS_ID_WRLINUX,         // linux
  MKC_ISYS_ID_MAX,
} mkc_system_id_t;

typedef enum {
  MKC_ILIB_LOC_LIB,
  MKC_ILIB_LOC_LIB64,
  MKC_ILIB_LOC_NOTSET,
} mkc_lib_loc_t;

typedef enum {
  MKC_IHEADER_LEGACY = false,
  MKC_IHEADER_MODERN = true,
} mkc_header_t;

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_DEF_H */
