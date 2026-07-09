#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

MAKEFLAGS += --no-print-directory
MAKEFILE = bootstrap.mk
# if pkgconf is installed, there is a pkg-config symlink
# macports pkgconf does not appear to work properly
PKGCONF = pkg-config

MKC_REGEX_PKG = libpcre2-8
MKC_REGEX_OBJ = mkc_regex_pcre.o
MKC_REGEX_SRC = mkc_regex_pcre.c

MAIN_TMP = tmp
BOOTSTRAP_INIT = ${MAIN_TMP}/bootstrap-init.txt
BOOTSTRAP_NOREGEX = ${MAIN_TMP}/bootstrap-noregex.txt
BOOTSTRAP_TMPDIR = ${MAIN_TMP}/bootstrap-tmpdir.txt
BOOTSTRAP_MKC_FILES = ${MAIN_TMP}/bootstrap-mkcfiles.txt
MKC_FILES = mkc_files
MKC_TMP = mkc_files/tmp

BASE_OPTFLAGS = -g -O2
BASE_CFLAGS = -Wall -Wextra -Wno-unused-parameter \
	-I. -Iinclude
BASE_LDFLAGS = -Wall -Wextra
BASE_LIBS =

INIT_DEF = -DMKC_BOOTSTRAP=1

CONF_CFLAGS = $(BASE_OPTFLAGS) $(BASE_CFLAGS) \
    $$($(PKGCONF) --cflags $(MKC_REGEX_PKG))
CONF_LDFLAGS = $(BASE_OPTFLAGS) $(BASE_LDFLAGS)
CONF_LIBS = $$($(PKGCONF) --libs $(MKC_REGEX_PKG))

DBG_OPTFLAGS = -g -ggdb3 -O0
DBG_CFLAGS = $(DBG_OPTFLAGS) $(BASE_CFLAGS) \
    $$($(PKGCONF) --cflags $(MKC_REGEX_PKG))
DBG_LDFLAGS = $(DBG_OPTFLAGS) $(BASE_LDFLAGS)
DBG_LIBS = $$($(PKGCONF) --libs $(MKC_REGEX_PKG))

SAN_OPTFLAGS = -g -ggdb3 -Og
SAN_CFLAGS = $(SAN_OPTFLAGS) $(BASE_CFLAGS) \
    -Wno-unused-parameter \
    -fsanitize=address,undefined \
    -fsanitize-address-use-after-scope \
    -fsanitize-recover=address \
    -fno-omit-frame-pointer \
    -fno-common \
    -fno-inline \
    $$(pkgconf --cflags $(MKC_REGEX_PKG))
SAN_LDFLAGS = $(SAN_OPTFLAGS) $(BASE_LDFLAGS) \
    -fsanitize=address,undefined \
    -fsanitize-address-use-after-scope \
    -fsanitize-recover=address
SAN_LIBS = $$(pkgconf --libs $(MKC_REGEX_PKG)) -lrt


WIN=F
WINOBJ=mkc_os_win_process.o

.PHONY: start
start:
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CONF_CFLAGS)" \
	    LDFLAGS="$(CONF_LDFLAGS)" \
	    LIBS="$(CONF_LIBS)" \
	    real-start

.PHONY: real-start
real-start:
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    bootstrap-final

# the initial pass
# there is no mkc_cnofig.h file; MKC_BOOTSTRAP must be defined
$(BOOTSTRAP_INIT): $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_FILES)
	@echo "make: -- bootstrap mkc (initial)"
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS) $(INIT_DEF)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    TARGET=all oscheck
	@touch $(BOOTSTRAP_INIT)

# the second time through, the mkc_config.h file has been
# created, but regular expressions were not available, therefore
# any check that uses regular expressions did not work
# (arg-count, shell-extract)
# re-compile any module that uses mkc_config.h
# and re-build mkc_config.h
# the mkc_config.h that is created is now correct and complete
$(BOOTSTRAP_NOREGEX): $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_FILES) \
		$(BOOTSTRAP_INIT)
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    bootstrap-noregex

.PHONY: bootstrap-noregex
bootstrap-noregex: mkc_config.h
	@echo "make: -- bootstrap mkc (noregex)"
	@$(MAKE) -f $(MAKEFILE) initialclean
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    TARGET=all oscheck
	@# make sure mkc_config.h is re-built
	@rm -f mkc_config.h
	@echo "make: -- bootstrap mkc (prep-final)"
	@$(MAKE) -f $(MAKEFILE) mkc_config.h
	@# prep for the bootstrap-final target
	@$(MAKE) -f $(MAKEFILE) noregexclean
	@touch $(BOOTSTRAP_NOREGEX)

# now that a correct and complete mkc_config.h file has been
# created, re-compile once more
.PHONY: bootstrap-final
bootstrap-final: $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_FILES) \
		$(BOOTSTRAP_NOREGEX)
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    TARGET=all oscheck

.PHONY: sanitize
sanitize:
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(SAN_CFLAGS)" \
	    LDFLAGS="$(SAN_LDFLAGS)" \
	    LIBS="$(SAN_LIBS)" \
	    real-start

.PHONY: debug
debug:
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(DBG_CFLAGS)" \
	    LDFLAGS="$(DBG_LDFLAGS)" \
	    LIBS="$(DBG_LIBS)" \
	    real-start

mkc_config.h: mkc.mkc
	@./mkc --no-cache mkc.mkc

$(BOOTSTRAP_TMPDIR):
	@test -d $(MAIN_TMP) || mkdir $(MAIN_TMP)
	@touch $(BOOTSTRAP_TMPDIR)

$(BOOTSTRAP_MKC_FILES):
	@test -d $(MKC_FILES) || mkdir $(MKC_FILES)
	@test -d $(MKC_TMP) || mkdir $(MKC_TMP)
	@touch $(BOOTSTRAP_MKC_FILES)

# only clean the object files that are dependent on mkc_config.h
# do not clean the mkc_files/ directory
.PHONY: initialclean
initialclean:
	@rm -f $(INITIALOBJ) mkc

# only clean the object files that are dependent on mkc_config.h
# do not clean the mkc_files/ directory
.PHONY: noregexclean
noregexclean:
	@rm -f $(NOREGEXOBJ) mkc

.PHONY: oscheck
oscheck:
	@if [ `./utils/chkforwin.sh` = T ]; then \
	  $(MAKE) -f $(MAKEFILE) windows-$(TARGET) ; \
	else \
	  $(MAKE) -f $(MAKEFILE) other-$(TARGET) ; \
	fi

.PHONY: windows-all
windows-all:
	@# _WIN32 does not seem to be defined under cygwin
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS) -DMKC_SYS_WIN=1" \
	    MKC_WIN_OBJ="$(WINOBJ)" real-all

.PHONY: other-all
other-all:
	@$(MAKE) -f $(MAKEFILE) real-all

.PHONY: real-all
real-all: mkc topochk

# the MKCOBJECTS variable is re-generated
# be sure it is followed by a blank line
# MKCOBJECTS keep this line
MKCOBJECTS = \
	mkc_main.o \
	mkc_grammar.o \
	mkc_dirop.o \
	mkc_parse.o \
	mkc_ast.o \
	mkc_lex.o \
	mkc_process.o \
	mkc_toposort.o \
	mkc_dirmatch.o \
	mkc_context.o \
	mkc_check.o \
	mkc_asttoken.o \
	mkc_dirlist.o \
	$(MKC_REGEX_OBJ) \
	mkc_pvar.o \
	mkc_path.o \
	mkc_os_process.o \
	$(MKC_WIN_OBJ) \
	mkc_profile.o \
	mkc_env.o \
	mkc_tmutil.o \
	mkc_var.o \
	mkc_compiler.o \
	mkc_log.o \
	mkc_list.o \
	mkc_const.o \
	mkc_fileop.o \
	mkc_error.o \
	mkc_string.o

TOPOCHKOBJ = \
	topochk.o \
	mkc_toposort.o \
	mkc_list.o \
	mkc_string.o \
	mkc_error.o

# the INITIALOBJ variable will be replaced
# be sure it is followed by a blank line
# INITIALOBJ keep this line
INITIALOBJ = \
	mkc_grammar.o \
	mkc_check.o \
	mkc_dirlist.o \
	mkc_dirmatch.o \
	mkc_dirop.o \
	mkc_env.o \
	mkc_fileop.o \
	mkc_main.o \
	mkc_os_process.o \
	mkc_os_win_process.o \
	mkc_path.o \
	mkc_process.o \
	mkc_regex_pcre.o \
	mkc_string.o \
	mkc_tmutil.o

# the NOREGEXOBJ variable will be replaced
# be sure it is followed by a blank line
# NOREGEXOBJ keep this line
NOREGEXOBJ = \
	mkc_dirop.o \
	mkc_main.o \
	mkc_process.o

mkc: $(MKCOBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(MKCOBJECTS) $(LIBS)

topochk: $(TOPOCHKOBJ)
	$(CC) $(LDFLAGS) -o $@ $(TOPOCHKOBJ)

# source

mkc_grammar.h mkc_grammar.c: mkc_grammar.y
	bison -d -v -o mkc_grammar.c --defines=mkc_grammar.h mkc_grammar.y

mkc_lex.h mkc_lex.c: mkc_lex.l
	flex --header-file=mkc_lex.h -o mkc_lex.c mkc_lex.l

# objects

.c.o:
	$(CC) -c $(CFLAGS) $<

mkc_ast.o: mkc_ast.c
mkc_asttoken.o: mkc_asttoken.c
mkc_check.o: mkc_check.c
mkc_compiler.o: mkc_compiler.c
mkc_const.o: mkc_const.c
mkc_context.o: mkc_context.c
mkc_dirop.o: mkc_dirop.c
mkc_dirlist.o: mkc_dirlist.c
mkc_dirmatch.o: mkc_dirmatch.c
mkc_env.o: mkc_env.c
mkc_fileop.o: mkc_fileop.c
mkc_grammar.o: mkc_grammar.c
mkc_lex.o: mkc_lex.c mkc_grammar.h
mkc_list.o: mkc_list.c
mkc_log.o: mkc_log.c
mkc_main.o: mkc_main.c
mkc_os_process.o: mkc_os_process.c
mkc_os_win_process.o: mkc_os_win_process.c
mkc_parse.o: mkc_parse.c mkc_grammar.h mkc_lex.h
mkc_path.o: mkc_path.c
mkc_process.o: mkc_process.c
mkc_profile.o: mkc_profile.c
mkc_pvar.o: mkc_pvar.c
$(MKC_REGEX_OBJ): $(MKC_REGEX_SRC)
mkc_string.o: mkc_string.c
mkc_tmutil.o: mkc_tmutil.c
mkc_toposort.o: mkc_toposort.c
mkc_var.o: mkc_var.c

topochk.o: topochk.c

# DO NOT DELETE

mkc_ast.o:   include/mkc_ast.h
mkc_ast.o: include/mkc_asttoken.h include/mkc_error.h include/mkc_nodiscard.h
mkc_ast.o: include/mkc_log.h  include/mkc_option.h
mkc_ast.o: include/mkc_var.h include/mkc_list.h include/mkc_check.h
mkc_ast.o: include/mkc_compiler.h include/mkc_profile.h include/mkc_pvar.h
mkc_ast.o: include/mkc_context.h include/mkc_def.h 
mkc_ast.o:  include/mkc_os_process.h
mkc_ast.o: include/mkc_process.h include/mkc_string.h
mkc_asttoken.o: include/mkc_asttoken.h
mkc_check.o: include/mkc_check.h include/mkc_compiler.h include/mkc_error.h
mkc_check.o:  include/mkc_nodiscard.h include/mkc_list.h
mkc_check.o: include/mkc_log.h  include/mkc_profile.h
mkc_check.o: include/mkc_option.h include/mkc_var.h include/mkc_pvar.h
mkc_check.o: include/mkc_const.h include/mkc_def.h 
mkc_check.o:  include/mkc_env.h include/mkc_fileop.h
mkc_check.o: include/mkc_os_process.h include/mkc_path.h include/mkc_regex.h
mkc_check.o: include/mkc_string.h
mkc_compiler.o:  include/mkc_compiler.h
mkc_context.o:  include/mkc_context.h
mkc_context.o: include/mkc_error.h include/mkc_nodiscard.h
mkc_dirlist.o:   include/mkc_def.h
mkc_dirlist.o: include/mkc_dirlist.h include/mkc_error.h
mkc_dirlist.o: include/mkc_nodiscard.h include/mkc_list.h
mkc_dirlist.o: include/mkc_fileop.h  include/mkc_string.h
mkc_dirmatch.o: include/mkc_dirlist.h include/mkc_error.h
mkc_dirmatch.o:  include/mkc_nodiscard.h
mkc_dirmatch.o: include/mkc_list.h include/mkc_dirmatch.h include/mkc_regex.h
mkc_dirop.o: include/mkc_def.h  
mkc_dirop.o: include/mkc_dirop.h include/mkc_error.h include/mkc_nodiscard.h
mkc_dirop.o: include/mkc_fileop.h  include/mkc_string.h
mkc_env.o:   include/mkc_env.h
mkc_env.o: include/mkc_string.h include/mkc_nodiscard.h
mkc_error.o:  include/mkc_error.h include/mkc_nodiscard.h
mkc_error.o: include/mkc_string.h
mkc_fileop.o: include/mkc_def.h include/mkc_error.h include/mkc_nodiscard.h
mkc_fileop.o: include/mkc_fileop.h include/mkc_string.h
mkc_grammar.o: mkc_grammar.h  
mkc_grammar.o: include/mkc_ast.h include/mkc_asttoken.h include/mkc_error.h
mkc_grammar.o: include/mkc_nodiscard.h include/mkc_log.h 
mkc_grammar.o: include/mkc_option.h include/mkc_var.h include/mkc_list.h
mkc_grammar.o: include/mkc_def.h  
mkc_grammar.o: include/mkc_fileop.h include/mkc_parse.h
mkc_lex.o:   mkc_grammar.h
mkc_lex.o:  include/mkc_ast.h
mkc_lex.o: include/mkc_asttoken.h include/mkc_error.h include/mkc_nodiscard.h
mkc_lex.o: include/mkc_log.h  include/mkc_option.h
mkc_lex.o: include/mkc_var.h include/mkc_list.h include/mkc_def.h
mkc_lex.o:   include/mkc_fileop.h
mkc_lex.o: include/mkc_parse.h 
mkc_list.o:   include/mkc_error.h
mkc_list.o: include/mkc_nodiscard.h include/mkc_list.h include/mkc_string.h
mkc_log.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_fileop.h
mkc_log.o:  include/mkc_log.h include/mkc_string.h
mkc_main.o:   include/mkc_ast.h
mkc_main.o: include/mkc_asttoken.h include/mkc_error.h
mkc_main.o: include/mkc_nodiscard.h include/mkc_log.h 
mkc_main.o: include/mkc_option.h include/mkc_var.h include/mkc_list.h
mkc_main.o: include/mkc_const.h include/mkc_def.h 
mkc_main.o:  include/mkc_dirop.h include/mkc_env.h
mkc_main.o: include/mkc_fileop.h include/mkc_parse.h include/mkc_path.h
mkc_main.o: include/mkc_profile.h include/mkc_compiler.h include/mkc_string.h
mkc_main.o: include/mkc_tmutil.h
mkc_os_process.o:  include/mkc_os_process.h
mkc_os_process.o: include/mkc_nodiscard.h include/mkc_tmutil.h
mkc_os_win_process.o:  include/mkc_def.h
mkc_os_win_process.o:  include/mkc_fileop.h
mkc_os_win_process.o: include/mkc_error.h include/mkc_nodiscard.h
mkc_os_win_process.o: include/mkc_os_process.h include/mkc_string.h
mkc_os_win_process.o: include/mkc_tmutil.h
mkc_parse.o: include/mkc_ast.h include/mkc_asttoken.h include/mkc_error.h
mkc_parse.o: include/mkc_nodiscard.h include/mkc_log.h 
mkc_parse.o:  include/mkc_option.h
mkc_parse.o: include/mkc_var.h include/mkc_list.h include/mkc_fileop.h
mkc_parse.o: mkc_lex.h mkc_grammar.h 
mkc_parse.o:  include/mkc_def.h 
mkc_parse.o:  include/mkc_parse.h 
mkc_parse.o: include/mkc_string.h
mkc_path.o: include/mkc_def.h  
mkc_path.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_fileop.h
mkc_path.o:  include/mkc_path.h include/mkc_string.h
mkc_process.o: include/mkc_asttoken.h include/mkc_check.h
mkc_process.o: include/mkc_compiler.h include/mkc_error.h
mkc_process.o: include/mkc_nodiscard.h include/mkc_list.h include/mkc_log.h
mkc_process.o:  include/mkc_profile.h include/mkc_option.h
mkc_process.o: include/mkc_var.h include/mkc_pvar.h include/mkc_const.h
mkc_process.o: include/mkc_context.h include/mkc_dirmatch.h
mkc_process.o: include/mkc_regex.h include/mkc_def.h 
mkc_process.o: include/mkc_env.h include/mkc_fileop.h include/mkc_path.h
mkc_process.o: include/mkc_process.h include/mkc_string.h
mkc_process.o: include/mkc_tmutil.h include/mkc_toposort.h
mkc_profile.o: include/mkc_compiler.h include/mkc_const.h include/mkc_error.h
mkc_profile.o: include/mkc_nodiscard.h include/mkc_list.h
mkc_profile.o: include/mkc_option.h include/mkc_profile.h include/mkc_log.h
mkc_profile.o:  include/mkc_var.h include/mkc_string.h
mkc_pvar.o:   include/mkc_def.h
mkc_pvar.o:   include/mkc_env.h
mkc_pvar.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_profile.h
mkc_pvar.o: include/mkc_compiler.h include/mkc_list.h include/mkc_log.h
mkc_pvar.o:  include/mkc_option.h include/mkc_var.h
mkc_pvar.o: include/mkc_pvar.h include/mkc_string.h
mkc_regex_pcre.o: include/mkc_def.h  include/mkc_error.h
mkc_regex_pcre.o: include/mkc_nodiscard.h include/mkc_regex.h
mkc_string.o: include/mkc_string.h include/mkc_nodiscard.h
mkc_tmutil.o:  include/mkc_tmutil.h
mkc_toposort.o: include/mkc_def.h  
mkc_toposort.o: include/mkc_error.h include/mkc_nodiscard.h
mkc_toposort.o: include/mkc_list.h include/mkc_string.h
mkc_toposort.o: include/mkc_toposort.h
mkc_var.o:   include/mkc_const.h
mkc_var.o: include/mkc_def.h  
mkc_var.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_list.h
mkc_var.o: include/mkc_var.h include/mkc_log.h 
mkc_var.o: include/mkc_string.h
topochk.o:  include/mkc_def.h 
topochk.o:  include/mkc_error.h include/mkc_nodiscard.h
topochk.o: include/mkc_toposort.h include/mkc_list.h include/mkc_string.h
