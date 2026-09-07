/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *    (from ballroomdj4)
 */

#ifndef MKC_BOOTSTRAP
# include "mkc_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "dirop.h"
#include "dirmatch.h"
#include "mkc_error.h"
#include "list.h"
#include "mkc_nodiscard.h"
#include "mkc_regex.h"

#if _have_regex

MKC_NODISCARD
list_t *
dir_match (const char *dirname, mkc_regex_t *rx, mkc_error_t *mkcerr)
{
  list_t    *flist;
  list_t    *nflist;
  listidx_t fiteridx;
  listidx_t fidx;

  nflist = list_init (MKC_LIST_UNSORTED, list_ind_free, NULL, mkcerr);

  flist = dirop_basic_list (dirname, mkcerr);
  list_iter_start (flist, &fiteridx);
  while ((fidx = list_iter_next (flist, &fiteridx)) != MKC_ITER_FINISH) {
    char          **temp;
    char          *fn;

    if (mkc_error_chk_err (mkcerr)) {
      break;
    }

    temp = list_get_by_idx (flist, fidx);
    fn = *temp;
    if (mkc_regex_match (rx, fn)) {
      char    *tp;

      tp = strdup (fn);
      list_set (nflist, &tp, sizeof (char *));
    }
  }

  list_free (flist);
  return nflist;
}

#endif
