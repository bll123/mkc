/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mkc_compiler.h"
#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_log.h"
#include "mkc_var.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef mkc_listidx_t mkc_profidx_t;
typedef struct mkc_profile_t mkc_profile_t;

typedef enum {
  MKC_PROF_TYPE_INTERNAL,
  MKC_PROF_TYPE_GLOBAL,
  MKC_PROF_TYPE_USER,
  MKC_PROF_TYPE_TARGET,
  MKC_PROF_TYPE_LOCAL,
  MKC_PROF_TYPE_INVALID,
} mkc_prof_type_t;

enum {
  MKC_PROF_NOT_FOUND = -2,
};

extern const char * const MKC_PROF_GLOBAL_NAME;
extern const char * const MKC_PROF_INTERNAL_NAME;
extern const char * const MKC_PROF_RELEASE_NAME;

mkc_profile_t * mkc_profile_init (mkc_log_t *log, mkc_error_t *mkcerr, const char *dfltprof, const char *comparg);
void mkc_profile_free (mkc_profile_t *profiles);
int mkc_profile_clear (mkc_profile_t *profiles, mkc_profidx_t pidx);
mkc_compiler_t mkc_profile_get_dflt_compiler (mkc_profile_t *profiles);
void mkc_profile_set_dflt_compiler (mkc_profile_t *profiles, mkc_compiler_t compiler);

mkc_profidx_t mkc_profile_find (mkc_profile_t *profiles, const char *pname, const char *compid);
mkc_profidx_t mkc_profile_find_id (mkc_profile_t *profiles, const char *pname, mkc_compiler_t compiler);
mkc_profidx_t mkc_profile_create (mkc_profile_t *profiles, const char *pname, const char *comptxt, mkc_prof_type_t);
int mkc_profile_local_create (mkc_profile_t *profiles);
void mkc_profile_local_pop (mkc_profile_t *profiles);

mkc_varlist_t *mkc_profile_get_varlist (mkc_profile_t *profiles, mkc_profidx_t pidx);

void mkc_profile_iter_start (mkc_profile_t *profiles, mkc_profidx_t *iteridx);
mkc_profidx_t mkc_profile_iter_next (mkc_profile_t *profiles, mkc_profidx_t *iteridx);

const char * mkc_profile_get_name (mkc_profile_t *profiles, mkc_profidx_t pidx);
mkc_compiler_t mkc_profile_get_compiler (mkc_profile_t *profiles, mkc_profidx_t pidx);
const char * mkc_profile_get_comp_name (mkc_profile_t *profiles, mkc_profidx_t pidx);
mkc_prof_type_t mkc_profile_get_type (mkc_profile_t *profiles, mkc_profidx_t pidx);

mkc_profidx_t mkc_profile_push (mkc_profile_t *profiles);
mkc_profidx_t mkc_profile_pop (mkc_profile_t *profiles);

void mkc_profile_set_active (mkc_profile_t *profiles, mkc_profidx_t pidx);
mkc_profidx_t mkc_profile_get_active (mkc_profile_t *profiles);

void mkc_profile_local_reset (mkc_profile_t *profiles);
mkc_profidx_t mkc_profile_next (mkc_profile_t *profiles, mkc_compiler_t origcompiler);
const char * mkc_profile_curr_disp (mkc_profile_t *profiles, char *buff, size_t sz);

bool mkc_profile_is_current (mkc_profile_t *profiles, const char *name);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif
