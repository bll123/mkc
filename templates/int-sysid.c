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

  /* note that only major flavors are of concern, not every sub-flavor */

  /* /etc/os-release is rather dependent on the os that created it */
  /* but see what we can do */
  /* examples */
  /*   alpine */
  /*     ID=alpine */
  /*   devuan */
  /*     ID=devuan */
  /*     ID_LIKE=debian */
  /*   fedora */
  /*     ID=fedora */
  /*   manjaro */
  /*     ID=manjaro */
  /*     ID_LIKE=arch */
  /*   linux mint */
  /*     ID=linuxmint */
  /*     ID_LIKE="ubuntu debian" (ack!) */
  /*   mx-linux */
  /*     ID=debian */
  /*   redhat (old example found on internet) */
  /*     ID="rhel" */
  /*     ID_LIKE="fedora" */
  /*   suse */
  /*     ID="opensuse-leap" */
  /*     ID_LIKE="suse opensuse" */
  /*   ubuntu */
  /*     ID=ubuntu */
  /*     ID_LIKE=debian */

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
    if (strcmp (p, "arch") == 0) {
      sysid = MKC_SYS_ID_ARCH;
    }
    /* compare to ubuntu to handle linux-mint os-release */
    if (strcmp (p, "debian") == 0 ||
        strcmp (p, "ubuntu") == 0) {
      sysid = MKC_SYS_ID_DEBIAN;
    }
    if (strcmp (p, "fedora") == 0) {
      sysid = MKC_SYS_ID_FEDORA;
    }
    if (strcmp (p, "suse") == 0) {
      sysid = MKC_SYS_ID_SUSE;
    }
  }

  fclose (fh);

  return sysid;
}
