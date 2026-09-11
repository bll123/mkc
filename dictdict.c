/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 *
 * nested dictionary
 * used for the build data.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "dict.h"
#include "dictdict.h"
#include "mkc_def.h"
#include "mkc_error.h"
#include "value.h"

void
dictdict_set (dict_t * dd,
    const char * name, const char * tag, value_t *value,
    mkc_log_t * log, list_free_t freefunc,
    mkc_error_t *mkcerr)
{
  value_t   * subdval;
  dict_t    * subd;

  if (dd == NULL) {
    return;
  }
  if (name == NULL || tag == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_NULL_ARGUMENT, 0, name);
    return;
  }

  subdval = dict_get (dd, name);
  if (subdval == NULL) {
    value_t   tvalue;

    subd = dict_init (log, freefunc, sizeof (value_t), mkcerr);
    value_init (&tvalue);
    tvalue.vtype = MKC_VT_DICT;
    tvalue.dict = subd;
    tvalue.vctxt = MKC_VCTXT_MKC;
    dict_set (dd, name, &tvalue);
    subdval = dict_get (dd, name);
  }

  dict_set (subdval->dict, tag, value);
}

value_t *
dictdict_get (dict_t * dd, const char * name, const char *tag,
    mkc_error_t *mkcerr)
{
  value_t   * value;
  value_t   * subdval;

  if (name == NULL || tag == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_NULL_ARGUMENT, 0, name);
    return NULL;
  }

  subdval = dict_get (dd, name);
  if (subdval == NULL || subdval->vtype != MKC_VT_DICT) {
    return NULL;
  }
  value = dict_get (subdval->dict, tag);

  return value;
}

void
dictdict_delete (dict_t * dd, const char * name, const char *tag,
    mkc_error_t *mkcerr)
{
  value_t   * value;
  value_t   * subdval;

  if (name == NULL || tag == NULL) {
    mkc_error_set (mkcerr, MKC_ERR_NULL_ARGUMENT, 0, name);
    return;
  }

  subdval = dict_get (dd, name);
  if (subdval == NULL || subdval->vtype != MKC_VT_DICT) {
    return;
  }

  dict_delete (subdval->dict, tag);
}
