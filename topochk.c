/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mkc_def.h"
#include "mkc_error.h"
#include "toposort.h"
#include "mkc_string.h"

int
main (int argc, char *argv [])
{
  toposort_t  *topo;
  mkc_error_t     *mkcerr;
  FILE            *fh;
  char            buff [MKC_VNAME_MAX];
  const char      *nm;
  int             rc;

  mkcerr = mkc_error_init ();

  fh = fopen (argv [1], "r");
  if (fh == NULL) {
    fprintf (stderr, "unable to open %s\n", argv [1]);
    exit (1);
  }

  topo = toposort_init (mkcerr);

  while (fgets (buff, sizeof (buff), fh) != NULL) {
    mkc_strtrim (buff, 0);

    toposort_add_item (topo, buff);
  }

  fclose (fh);

  fh = fopen (argv [2], "r");
  if (fh == NULL) {
    fprintf (stderr, "unable to open %s\n", argv [2]);
    exit (1);
  }

  while (fgets (buff, sizeof (buff), fh) != NULL) {
    char      *p;

    mkc_strtrim (buff, 0);
    p = strstr (buff, " ");
    if (p == NULL) {
      continue;
    }
    *p = '\0';
    p += 1;

    rc = toposort_add_pair (topo, buff, p);
    if (rc != MKC_OK) {
      mkc_error_print (mkcerr);
      exit (1);
    }
  }

  fclose (fh);

  rc = toposort (topo);

  fprintf (stderr, "-- sorted\n");
  toposort_iter_start (topo);
  while ((nm = toposort_iter_next (topo)) != NULL) {
    fprintf (stderr, "  %s\n", nm);
  }
  if (rc == MKC_ERR_FAILURE) {
    fprintf (stderr, "-- cycle\n");
    toposort_disp_cycle (topo, buff, sizeof (buff));
    fprintf (stderr, "  %s\n", buff);
  }

  mkc_error_free (mkcerr);
  toposort_free (topo);
  return 0;
}
