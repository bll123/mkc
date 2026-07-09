/*
 * Copyright 2026 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MKC_CHECK_H
#define INC_MKC_CHECK_H

#include <stddef.h>
#include <stdbool.h>

#include "mkc_compiler.h"
#include "mkc_error.h"
#include "mkc_list.h"
#include "mkc_log.h"
#include "mkc_nodiscard.h"
#include "mkc_profile.h"
#include "mkc_pvar.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

/* each alternate will have a name associated with it */
/* the first alternate in the list is the base context */
/* and stores the name for the check */
typedef struct mkc_alternate_t {
  char            * name;
  mkc_list_t      * hdrlist;
  mkc_list_t      * compflags;
  mkc_list_t      * linkflags;
} mkc_alternate_t;

typedef enum {
  MKC_ATTR_INPUT,
  MKC_ATTR_LIB_VERSION,
  MKC_ATTR_MATCH,
  MKC_ATTR_METHOD,
  MKC_ATTR_OUTPUT,
  MKC_ATTR_VCONTEXT,
  MKC_ATTR_VERSION,
  MKC_ATTR_MAX,
} mkc_attr_type_t;

typedef struct mkc_attribute_t {
  mkc_list_t      * alternates;
  mkc_alternate_t * curralt;
  char            * currname;
  char            * str [MKC_ATTR_MAX];
  mkc_list_t      * pathlist;
  mkc_list_t      * replacelist;
  mkc_compiler_t  currcompiler;
  int             define_zero;
  int             headertype;
  bool            negate;
} mkc_attribute_t;

typedef struct mkc_check_t mkc_check_t;

MKC_NODISCARD mkc_check_t *mkc_check_init (mkc_profile_t *profiles, mkc_pvar_t *pvar, mkc_attribute_t *attr, mkc_log_t *log, mkc_profidx_t pixd_global_general, mkc_error_t *mkcerr);
void mkc_check_free (mkc_check_t *check);

int mkc_create_dirs (void);
mkc_err_code_t mkc_chk_compiler_env (mkc_check_t *check);
int mkc_chk_compiler_works (mkc_check_t *check, mkc_compiler_t compiler);
int mkc_chk_compiler_id (mkc_check_t *check, mkc_compiler_t compiler);

void mkc_chk_reset (mkc_check_t *check);
void mkc_chk_append_comp_flag (mkc_check_t *check, const char *flag);
void mkc_chk_append_link_flag (mkc_check_t *check, const char *flag);

/* internal checks */
int mkc_chk_header_modern (mkc_check_t *check, mkc_compiler_t compiler);
int mkc_chk_library_location (mkc_check_t *check, mkc_compiler_t compiler);
int mkc_chk_system_id (mkc_check_t *check, mkc_compiler_t compiler);
int mkc_chk_system_type (mkc_check_t *check, mkc_compiler_t compiler);
int mkc_chk_variadic_macro (mkc_check_t *check, mkc_compiler_t compiler);

/* user checks */
int mkc_chk_arg_count (mkc_check_t *check, mkc_compiler_t compiler, const char *funcname);
int mkc_chk_compiler_flag (mkc_check_t *check, mkc_compiler_t compiler, const char *flag, bool negate);
int mkc_chk_const (mkc_check_t *check, mkc_compiler_t compiler, const char *consttxt);
int mkc_chk_define (mkc_check_t *check, mkc_compiler_t compiler, const char *def);
int mkc_chk_function (mkc_check_t *check, mkc_compiler_t compiler, const char *funcname);
int mkc_chk_header (mkc_check_t *check, mkc_compiler_t compiler, const char *header);
int mkc_chk_link_flag (mkc_check_t *check, mkc_compiler_t compiler, const char *flag);
int mkc_chk_package (mkc_check_t *check, mkc_compiler_t compiler, const char *pkg);
int mkc_chk_size (mkc_check_t *check, mkc_compiler_t compiler, const char *type);
int mkc_chk_struct_member (mkc_check_t *check, mkc_compiler_t compiler, const char *structname, const char *membername);
int mkc_chk_type (mkc_check_t *check, mkc_compiler_t compiler, const char *type);

void mkc_check_get_include_deps (mkc_check_t *check, mkc_compiler_t compiler, const char *fn, mkc_list_t *deplist);

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /* INC_MKC_CHECK_H */
