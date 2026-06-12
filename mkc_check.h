/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "mkc_def.h"
#include "mkc_env.h"
#include "mkc_log.h"
#include "mkc_profile.h"
#include "mkc_pvar.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef struct mkc_check_t mkc_check_t;

extern const char * const mkctestcompflags;

mkc_check_t *mkc_check_init (mkc_profile_t *profiles, mkc_pvar_t *pvar, mkc_log_t *log, mkc_error_t *mkcerr);
void mkc_check_free (mkc_check_t *check);

int mkc_create_dirs (void);
void mkc_chk_compiler_env (mkc_check_t *check);
int mkc_chk_compiler_works (mkc_check_t *check, const char *compiler, const char *sfx);
int mkc_chk_which_compiler (mkc_check_t *check, const char *compiler, const char *sfx);
const char * mkc_chk_guess_suffix (const char *ccstr);
const char * mkc_chk_compiler_suffix (int compiler);
int mkc_chk_compiler_id (mkc_check_t *check, const char *compiler, const char *sfx);
int mkc_chk_header_modern (mkc_check_t *check, const char *compiler, const char *sfx);
int mkc_chk_system_type (mkc_check_t *check, const char *compiler, const char *sfx);
int mkc_chk_system_id (mkc_check_t *check, const char *compiler, const char *sfx);
int mkc_chk_variadic_macro (mkc_check_t *check, const char *compiler, const char *sfx);
int mkc_chk_library_location (mkc_check_t *check, const char *compiler, const char *sfx);

int mkc_chk_compiler_flag (mkc_check_t *check, const char *compiler, const char *sfx, const char *flag, int negate);
int mkc_chk_link_flag (mkc_check_t *check, const char *compiler, const char *sfx, const char *flag);
int mkc_chk_size (mkc_check_t *check, const char *compiler, const char *sfx, const char *type);
int mkc_chk_type (mkc_check_t *check, const char *compiler, const char *sfx, const char *type);
int mkc_chk_struct_member (mkc_check_t *check, const char *compiler, const char *sfx, const char *structname, const char *membername);
int mkc_chk_function (mkc_check_t *check, const char *compiler, const char *sfx, const char *funcname);

int mkc_compile_only (mkc_check_t *check, const char *compiler, const char *sfx, const char *fname, const char *incpath, const char *flags []);
int mkc_compile_link (mkc_check_t *check, const char *compiler, const char *sfx, const char *fname, const char *incpath, const char *flags [], char *rbuff, size_t rsz);
int mkc_compile_run (mkc_check_t *check, const char *compiler, const char *sfx, const char *fname, const char *incpath, const char *flags [], char *rbuff, size_t rsz);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

