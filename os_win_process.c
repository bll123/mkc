/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *  (from ballroomdj4)
 */

#ifndef MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

#include "mkc_def.h"
#include "fileop.h"
#include "os_process.h"
#include "strutil.h"
#include "tmutil.h"

static void mkc_os_win_create_cmd (char *buff, size_t sz, const char *targv []);

pid_t
os_process_start (const char *targv[], int flags, char *outfname)
{
  pid_t               pid;
  STARTUPINFOW        si;
  PROCESS_INFORMATION pi;
  char                *buff;
  DWORD               val;
  BOOL                inherit = FALSE;
  wchar_t             *wbuff = NULL;
  wchar_t             *woutfname = NULL;
  HANDLE              outhandle = INVALID_HANDLE_VALUE;

  memset (&si, '\0', sizeof (si));
  si.cb = sizeof (si);
  memset (&pi, '\0', sizeof (pi));

  if (outfname != NULL && *outfname) {
    SECURITY_ATTRIBUTES sao;

    sao.nLength = sizeof (SECURITY_ATTRIBUTES);
    sao.lpSecurityDescriptor = NULL;
    sao.bInheritHandle = 1;

    woutfname = str_towide (outfname);
    outhandle = CreateFileW (woutfname,
      GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      &sao,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL);
    si.hStdOutput = outhandle;
    si.hStdError = outhandle;
    si.hStdInput = GetStdHandle (STD_INPUT_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    inherit = true;
  }

  buff = malloc (MKC_SMALL_BUFF_SZ);
  if (buff == NULL) {
    return -1;
  }

  mkc_os_win_create_cmd (buff, MKC_SMALL_BUFF_SZ, targv);
  wbuff = str_towide (buff);

  val = 0;
  /* how windows decides to create a command window is unknown. */
  /* there seem to be different rules for gui->console, console->console */
  if ((flags & OS_PROC_DETACH) == OS_PROC_DETACH) {
    val |= DETACHED_PROCESS;
    if ((flags & OS_PROC_WINDOW_OK) != OS_PROC_WINDOW_OK) {
      val |= CREATE_NO_WINDOW;
    }
  }
  if ((flags & OS_PROC_NOWINDOW) == OS_PROC_NOWINDOW) {
    val |= CREATE_NO_WINDOW;
  }

  if (! CreateProcessW (
      NULL,           // module name
      wbuff,           // command line
      NULL,           // process handle
      NULL,           // hread handle
      inherit,        // handle inheritance
      val,            // set to DETACHED_PROCESS
      NULL,           // parent's environment
      NULL,           // parent's starting directory
      &si,            // STARTUPINFO structure
      &pi )           // PROCESS_INFORMATION structure
  ) {
    long err = GetLastError ();
    fprintf (stderr, "osprocessstart: getlasterr-a: %ld %s\n", err, buff);
    free (buff);
    return -1;
  }

  pid = pi.dwProcessId;
  CloseHandle (pi.hThread);

  if ((flags & OS_PROC_WAIT) == OS_PROC_WAIT) {
    DWORD rc;

    WaitForSingleObject (pi.hProcess, INFINITE);
    GetExitCodeProcess (pi.hProcess, &rc);
    pid = rc;
  }

  if (outfname != NULL && *outfname) {
    bool            rc;
    int             count;
    struct __stat64 statbuf;

    CloseHandle (outhandle);
    rc = _wstat64 (woutfname, &statbuf);

    /* windows is mucked up; wait for the redirected output to appear */
    count = 0;
    while (rc == 0 && statbuf.st_size == 0 && count < 60) {
      mssleep (5);
      rc = _wstat64 (woutfname, &statbuf);
      ++count;
    }
  }
  if (woutfname != NULL) {
    free (woutfname);
  }
  free (wbuff);
  free (buff);
  return pid;
}

/* creates a pipe for re-direction and grabs the output */
int
os_process_pipe (const char *targv[], int flags, char *rbuff, size_t sz, size_t *retsz)
{
  STARTUPINFOW        si;
  PROCESS_INFORMATION pi;
  char                *buff;
  DWORD               val;
  wchar_t             *wbuff;
  HANDLE              handleStdoutRead = INVALID_HANDLE_VALUE;
  HANDLE              handleStdoutWrite = INVALID_HANDLE_VALUE;
  HANDLE              handleStdinRead = INVALID_HANDLE_VALUE;
  HANDLE              handleStdinWrite = INVALID_HANDLE_VALUE;
  SECURITY_ATTRIBUTES sao;
  DWORD               rbytes;
  DWORD               rc = 0;

  flags |= OS_PROC_WAIT;      // required

  memset (&si, '\0', sizeof (si));
  si.cb = sizeof (si);
  memset (&pi, '\0', sizeof (pi));

  sao.nLength = sizeof (SECURITY_ATTRIBUTES);
  sao.lpSecurityDescriptor = NULL;
  sao.bInheritHandle = TRUE;

  if ( ! CreatePipe (&handleStdoutRead, &handleStdoutWrite, &sao, 0) ) {
    long err = GetLastError ();
    fprintf (stderr, "createpipe: getlasterr-b: %ld\n", err);
    return -1;
  }
  if ( ! CreatePipe (&handleStdinRead, &handleStdinWrite, &sao, 0) ) {
    long err = GetLastError ();
    fprintf (stderr, "createpipe: getlasterr-c: %ld\n", err);
    return -1;
  }
  CloseHandle (handleStdinWrite);

  si.hStdOutput = handleStdoutWrite;
  if ((flags & OS_PROC_NOSTDERR) != OS_PROC_NOSTDERR) {
    si.hStdError = handleStdoutWrite;
  }
  if (rbuff != NULL) {
    si.hStdInput = GetStdHandle (STD_INPUT_HANDLE);
  } else {
    si.hStdInput = handleStdinRead;
  }
  si.dwFlags |= STARTF_USESTDHANDLES;

  buff = malloc (MKC_SMALL_BUFF_SZ);
  if (buff == NULL) {
    return -1;
  }

  mkc_os_win_create_cmd (buff, MKC_SMALL_BUFF_SZ, targv);
  wbuff = str_towide (buff);

  val = 0;
  if ((flags & OS_PROC_DETACH) == OS_PROC_DETACH) {
    val |= DETACHED_PROCESS;
    val |= CREATE_NO_WINDOW;
  }
  if ((flags & OS_PROC_NOWINDOW) == OS_PROC_NOWINDOW) {
    val |= CREATE_NO_WINDOW;
  }

  if (! CreateProcessW (
      NULL,           // module name
      wbuff,           // command line
      NULL,           // process handle
      NULL,           // hread handle
      TRUE,           // handle inheritance
      val,            // set to DETACHED_PROCESS
      NULL,           // parent's environment
      NULL,           // parent's starting directory
      &si,            // STARTUPINFO structure
      &pi )           // PROCESS_INFORMATION structure
  ) {
    long err = GetLastError ();
    fprintf (stderr, "osprocesspipe: getlasterr-d: %ld %s\n", err, buff);
    free (buff);
    return -1;
  }

  CloseHandle (pi.hThread);

  CloseHandle (handleStdoutWrite);

  {
    ssize_t bytesread = 0;
    bool    wait = true;

    if (rbuff != NULL) {
      rbuff [sz - 1] = '\0';
    }

    while (1) {
      if (rbuff != NULL) {
        ReadFile (handleStdoutRead, rbuff + bytesread, sz - bytesread, &rbytes, NULL);
        bytesread += rbytes;
        if (bytesread < (ssize_t) sz) {
          rbuff [bytesread] = '\0';
        }
        if (retsz != NULL) {
          *retsz = bytesread;
        }
      }
      if (! wait) {
        break;
      }
      if ((flags & OS_PROC_WAIT) == OS_PROC_WAIT) {
        if (WaitForSingleObject (pi.hProcess, 2) != WAIT_TIMEOUT) {
          GetExitCodeProcess (pi.hProcess, &rc);
          wait = false;
        }
      }
      if (wait) {
        Sleep (2);
      }
    }
    CloseHandle (handleStdoutRead);
    CloseHandle (pi.hProcess);
  }

  free (buff);
  free (wbuff);
  return rc;
}

static void
mkc_os_win_create_cmd (char *buff, size_t sz, const char *targv [])
{
  char      *p = buff;
  char      *tbuff;
  size_t    idx;

  /* windows and its stupid quoting issues */
  /* welcome to a nightmare */
  /* current logic: */
  /*    if there is a / or \ character in the argument, */
  /*        quote it with double quotes */
  /* example arguments: */
  /*    C:/Program Files/... */
  /*    -DABC="hello world" */
  /* this may need further changes */

  buff [0] = '\0';

  tbuff = malloc (MKC_PATH_MAX);
  if (tbuff == NULL) {
    return;
  }

  idx = 0;
  while (targv [idx] != NULL) {
    char    *sp = NULL;
    char    *bsp = NULL;

    sp = strchr (targv [idx], '/');
    if (sp == NULL) {
      bsp = strchr (targv [idx], '\\');
    }
    if (sp != NULL || bsp != NULL) {
      /* anything that contains a slash or backslash gets quoted, */
      /* as it may contain a space */
      snprintf (tbuff, sz, "\"%s\"", targv [idx]);
    } else {
      snprintf (tbuff, sz, "%s", targv [idx]);
    }
    p = stpecpy (p, buff + sz, tbuff);
    p = stpecpy (p, buff + sz, " ");
    ++idx;
  }
}
