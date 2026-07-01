#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include "mkc_def.h"

int
main (void)
{
  const char  *msystem = NULL;
  int         debexists = 1;
  struct stat statbuf;
  FILE        *fh = NULL;
  char        tbuff [1024];
  char        *p = NULL;
  int         sysid = MKC_SYS_ID_NOTSET;


#if defined (__CYGWIN__)
  return MKC_SYS_ID_CYGWIN;
#endif
#if defined (__FreeBSD__)
  return MKC_SYS_ID_FREEBSD;
#endif
#if defined (__NetBSD__)
  return MKC_SYS_ID_NETBSD;
#endif
#if defined (__OpenBSD__)
  return MKC_SYS_ID_OPENBSD;
#endif
#if defined (__DragonFly__)
  return MKC_SYS_ID_DRAGONFLYBSD;
#endif
#if defined (__SunOS)
  return MKC_SYS_ID_SOLARIS;
#endif

  msystem = getenv ("MSYSTEM");

  if (msystem != NULL) {
    sysid = MKC_SYS_ID_MSYS2;
  }

  debexists = stat ("/etc/debian_version", &statbuf);
  if (debexists == 0) {
    sysid = MKC_SYS_ID_DEBIAN;
  }

  if (sysid != MKC_SYS_ID_NOTSET) {
    return sysid;
  }

  /* note that only major variants are of concern, not every sub-variant */
  /* https://github.com/chef/os_release has a bunch of examples */

  /* /etc/os-release is rather dependent on the os that created it */
  /* but see what is possible */
  /* the ID_LIKE variable doesn't list the most basic type first, */
  /* so the comparisons have to check several names */

  fh = fopen ("/etc/os-release", "r");
  if (fh == NULL) {
    return sysid;
  }

  while (fgets (tbuff, sizeof (tbuff), fh) != NULL) {
    if (strncmp (tbuff, "ID=", 3) == 0) {
      p = tbuff + 3;
    }
    if (strncmp (tbuff, "ID_LIKE=", 8) == 0) {
      p = tbuff + 8;
      /* use ID_LIKE by preference */
      break;
    }
  }

  if (p != NULL) {
    char    *tp = NULL;
    char    *ep = NULL;

    if (*p == '"') {
      ++p;
    }

    tp = p;
    while (*tp) {
      if (*tp == ' ') {
        *tp = '\0';
        break;
      }
    }

    ep = tbuff + strlen (tbuff) - 1;
    while (ep >= tbuff &&
           (*ep == ' ' || *ep == '\r' || *ep == '\n' || *ep == '"')) {
      *ep = '\0';
      --ep;
    }

    if (strcmp (p, "alpine") == 0) {
      sysid = MKC_SYS_ID_ALPINE;
    }
    if (strcmp (p, "arch") == 0 ||
        strcmp (p, "cachyos") == 0 ||
        strcmp (p, "manjaro") == 0) {
      sysid = MKC_SYS_ID_ARCH;
    }
    /* compare to ubuntu to handle linux-mint os-release */
    if (strcmp (p, "debian") == 0 ||
        strcmp (p, "ubuntu") == 0) {
      sysid = MKC_SYS_ID_DEBIAN;
    }
    if (strcmp (p, "fedora") == 0 ||
        strcmp (p, "rhel") == 0 ||
        strcmp (p, "centos") == 0 ||
        strcmp (p, "mandriva") == 0) {
      sysid = MKC_SYS_ID_FEDORA;
    }
    if (strcmp (p, "gentoo") == 0) {
      sysid = MKC_SYS_ID_GENTOO;
    }
    if (strcmp (p, "nixos") == 0) {
      sysid = MKC_SYS_ID_NIXOS;
    }
    if (strcmp (p, "slackware") == 0) {
      sysid = MKC_SYS_ID_SLACKWARE;
    }
    if (strcmp (p, "suse") == 0 ||
        strcmp (p, "opensuse") == 0) {
      sysid = MKC_SYS_ID_SUSE;
    }
    if (strcmp (p, "wrlinux") == 0 ||
        strcmp (p, "cisco-wrlinux") == 0) {
      sysid = MKC_SYS_ID_WRLINUX;
    }
  }

  fclose (fh);

  return sysid;
}
