/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_OS_PROCESS_H
#define INC_MKC_OS_PROCESS_H

#include <stddef.h>
#include <unistd.h>

#include "mkc_nodiscard.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

enum {
  OS_PROC_NONE        = 0,
  OS_PROC_DETACH      = (1 << 0),
  OS_PROC_WAIT        = (1 << 1),
  OS_PROC_NOSTDERR    = (1 << 2),
  OS_PROC_NOWINDOW    = (1 << 3),
  OS_PROC_WINDOW_OK   = (1 << 4),
};

pid_t mkc_os_process_start (const char *targv[], int flags, char *outfname);
int mkc_os_process_pipe (const char *targv[], int flags, char *rbuff, size_t sz, size_t *retsz);
MKC_NODISCARD char * mkc_os_process_run (const char *prog, ...);

#endif /* INC_MKC_OS_PROCESS_H */
