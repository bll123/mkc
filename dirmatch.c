/*
 * Copyright 2021-2026 Brad Lanam Pleasant Hill CA
 *    (from ballroomdj4)
 */

#if ! defined (MKC_BOOTSTRAP)
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
#include "mkc_list.h"
#include "mkc_nodiscard.h"
#include "mkc_regex.h"

#if _have_regex

MKC_NODISCARD
mkc_list_t *
dir_match (const char *dirname, mkc_regex_t *rx, mkc_error_t *mkcerr)
{
  mkc_list_t    *flist;
  mkc_list_t    *nflist;
  mkc_listidx_t fiteridx;
  mkc_listidx_t fidx;

  nflist = mkc_list_init (MKC_LIST_UNSORTED, mkc_list_ind_free, NULL, mkcerr);

  flist = dirop_basic_list (dirname, mkcerr);
  mkc_list_iter_start (flist, &fiteridx);
  while ((fidx = mkc_list_iter_next (flist, &fiteridx)) != MKC_ITER_FINISH) {
    char          **temp;
    char          *fn;
    mkc_listidx_t loc;

    if (mkc_error_chk_err (mkcerr)) {
      break;
    }

    temp = mkc_list_get_by_idx (flist, fidx);
    fn = *temp;
    if (mkc_regex_match (rx, fn)) {
      char    *tp;

      tp = strdup (fn);
      mkc_list_set (nflist, &tp, sizeof (char *), &loc);
    }
  }

  mkc_list_free (flist);
  return nflist;
}

#endif
