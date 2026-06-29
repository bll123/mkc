#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

MAKEFLAGS += --no-print-directory
MAKEFILE = bootstrap.mk

BOOTSTRAP_INIT = tmp/bootstrap-init.txt
BOOTSTRAP_NOREGEX = tmp/bootstrap-noregex.txt
BOOTSTRAP_TMPDIR = tmp/bootstrap-tmpdir.txt
BOOTSTRAP_MKC_FILES = tmp/bootstrap-mkcfiles.txt
MKC_FILES = mkc_files
MKC_TMP = mkc_files/tmp
MAIN_TMP = tmp

BASE_OPTFLAGS = -g -O2
BASE_CFLAGS = -Wall -Wextra -Wno-unused-parameter \
	-Iinclude
BASE_LDFLAGS = -Wall -Wextra
BASE_LIBS = 

INIT_CFLAGS = $(BASE_OPTFLAGS) $(BASE_CFLAGS) -DMKC_BOOTSTRAP=1
INIT_LDFLAGS = $(BASE_LDFLAGS)
INIT_LIBS =

CONF_CFLAGS = $(BASE_OPTFLAGS) $(BASE_CFLAGS) $$(pkgconf --cflags glib-2.0)
CONF_LDFLAGS = $(BASE_LDFLAGS)
CONF_LIBS = $$(pkgconf --libs glib-2.0)

SAN_OPTFLAGS = -g -ggdb3 -Og
SAN_CFLAGS = $(SAN_OPTFLAGS) $(INIT_CFLAGS) \
    -Wno-unused-parameter \
    -fsanitize=address,undefined \
    -fsanitize-address-use-after-scope \
    -fsanitize-recover=address \
    -fno-omit-frame-pointer \
    -fno-common \
    -fno-inline
SAN_LDFLAGS = $(INIT_LDFLAGS) \
    -fsanitize=address,undefined \
    -fsanitize-address-use-after-scope \
    -fsanitize-recover=address \
    -lrt
SAN_LIBS =

WIN=F
WINOBJ=mkc_os_win_process.o

.PHONY: all
all:
	@$(MAKE) -f $(MAKEFILE) bootstrap-final

# the first time through, there is no mkc_config.h file
# MKC_BOOTSTRAP must be defined
$(BOOTSTRAP_INIT): $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_FILES)
	@echo "make: -- bootstrap mkc (initial)"
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(INIT_CFLAGS)" \
	    LDFLAGS="$(INIT_LDFLAGS)" \
	    LIBS="$(INIT_LIBS)" \
	    TARGET=all oscheck
	@touch $(BOOTSTRAP_INIT)

# the second time through, the mkc_config.h file has been 
# created, but regular expressions were not available, therefore
# any check that uses regular expressions did not work 
# (arg-count, shell-extract)
# re-compile any module that uses mkc_config.h
# and re-build mkc_config.h
# the mkc_config.h that is created is correct and complete
$(BOOTSTRAP_NOREGEX): $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_FILES) \
		$(BOOTSTRAP_INIT)
	@$(MAKE) -f $(MAKEFILE) bootstrap-noregex

.PHONY: bootstrap-noregex
bootstrap-noregex: mkc_config.h
	@echo "make: -- bootstrap mkc (noregex)"
	@$(MAKE) -f $(MAKEFILE) partialclean
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CONF_CFLAGS)" \
	    LDFLAGS="$(CONF_LDFLAGS)" \
	    LIBS="$(CONF_LIBS)" \
	    TARGET=all oscheck
	@# make sure mkc_config.h is re-built
	@rm -f mkc_config.h
	@$(MAKE) -f $(MAKEFILE) mkc_config.h
	@# prep for the bootstrap-final target
	@$(MAKE) -f $(MAKEFILE) partialclean
	@touch $(BOOTSTRAP_NOREGEX)

# now that a correct and complete mkc_config.h file has been
# created, re-compile once more
.PHONY: bootstrap-final
bootstrap-final: $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_FILES) \
		$(BOOTSTRAP_NOREGEX) mkc_config.h
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CONF_CFLAGS)" \
	    LDFLAGS="$(CONF_LDFLAGS)" \
	    LIBS="$(CONF_LIBS)" \
	    TARGET=all oscheck

mkc_config.h:
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
.PHONY: partialclean
partialclean:
	@rm -f $(PARTIALOBJ) mkc

.PHONY: sanitize
sanitize:
	$(MAKE) -f $(MAKEFILE) TARGET=all \
	    CFLAGS="$(SAN_CFLAGS)" LDFLAGS="$(SAN_LDFLAGS)" oscheck

.PHONY: oscheck
oscheck:
	@if [ `./utils/chkforwin.sh` = T ]; then \
	  $(MAKE) -f $(MAKEFILE) windows-$(TARGET) ; \
	else \
	  $(MAKE) -f $(MAKEFILE) other-$(TARGET) ; \
	fi

.PHONY: windows-all
windows-all:
	@$(MAKE) -f $(MAKEFILE) MKC_WIN_OBJ="$(WINOBJ)" real-all

.PHONY: other-all
other-all:
	@$(MAKE) -f $(MAKEFILE) real-all

.PHONY: real-all
real-all: mkc

MKCOBJECTS = mkc_main.o mkc_grammar.o \
	mkc_parse.o mkc_ast.o mkc_lex.o \
	mkc_process.o mkc_context.o \
        mkc_check.o mkc_asttoken.o \
	mkc_pvar.o mkc_os_process.o \
	$(MKC_WIN_OBJ) \
        mkc_profile.o mkc_env.o mkc_tmutil.o \
        mkc_var.o mkc_log.o \
        mkc_compiler.o mkc_list.o \
	mkc_regex.o \
	mkc_dirop.o mkc_osdir.o mkc_fileop.o mkc_path.o mkc_error.o \
	mkc_string.o 

# the PARTIALOBJ variable will be replaced
# be sure it is followed by a blank line
# PARTIALOBJ keep this line
PARTIALOBJ = \
	mkc_check.o \
	mkc_dirop.o \
	mkc_env.o \
	mkc_fileop.o \
	mkc_main.o \
	mkc_os_process.o \
	mkc_osdir.o \
	mkc_path.o \
	mkc_process.o \
	mkc_regex.o \
	mkc_string.o \
	mkc_tmutil.o

mkc: $(MKCOBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(MKCOBJECTS) $(LIBS)

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
mkc_context.o: mkc_context.c
mkc_dirop.o: mkc_dirop.c
mkc_env.o: mkc_env.c
mkc_fileop.o: mkc_fileop.c
mkc_grammar.o: mkc_grammar.c
mkc_lex.o: mkc_lex.c mkc_grammar.h
mkc_list.o: mkc_list.c
mkc_log.o: mkc_log.c
mkc_main.o: mkc_main.c
mkc_osdir.o: mkc_osdir.c
mkc_os_process.o: mkc_os_process.c
mkc_os_win_process.o: mkc_os_winprocess.c
mkc_parse.o: mkc_parse.c mkc_grammar.h mkc_lex.h
mkc_path.o: mkc_path.c
mkc_process.o: mkc_process.c
mkc_profile.o: mkc_profile.c
mkc_pvar.o: mkc_pvar.c
mkc_regex.o: mkc_regex.c
mkc_string.o: mkc_string.c
mkc_tmutil.o: mkc_tmutil.c
mkc_var.o: mkc_var.c

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
mkc_check.o:  include/mkc_nodiscard.h include/mkc_log.h
mkc_check.o:  include/mkc_profile.h include/mkc_list.h
mkc_check.o: include/mkc_option.h include/mkc_var.h include/mkc_pvar.h
mkc_check.o: include/mkc_def.h  
mkc_check.o: include/mkc_env.h include/mkc_fileop.h include/mkc_os_process.h
mkc_check.o: include/mkc_path.h include/mkc_regex.h include/mkc_string.h
mkc_compiler.o:  include/mkc_compiler.h
mkc_context.o:  include/mkc_context.h
mkc_context.o: include/mkc_error.h include/mkc_nodiscard.h
mkc_dirop.o:  include/mkc_def.h 
mkc_dirop.o:  include/mkc_dirop.h include/mkc_fileop.h
mkc_dirop.o:  include/mkc_error.h include/mkc_nodiscard.h
mkc_dirop.o: include/mkc_osdir.h include/mkc_string.h
mkc_env.o:  include/mkc_env.h include/mkc_string.h
mkc_env.o: include/mkc_nodiscard.h
mkc_error.o:  include/mkc_error.h include/mkc_nodiscard.h
mkc_error.o: include/mkc_string.h
mkc_fileop.o:   include/mkc_def.h
mkc_fileop.o: include/mkc_error.h include/mkc_nodiscard.h
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
mkc_main.o:  include/mkc_ast.h include/mkc_asttoken.h
mkc_main.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_log.h
mkc_main.o:  include/mkc_option.h include/mkc_var.h
mkc_main.o: include/mkc_list.h include/mkc_def.h 
mkc_main.o:  include/mkc_dirop.h include/mkc_env.h
mkc_main.o: include/mkc_fileop.h include/mkc_parse.h include/mkc_path.h
mkc_main.o: include/mkc_profile.h include/mkc_compiler.h include/mkc_string.h
mkc_main.o: include/mkc_tmutil.h
mkc_os_process.o:  include/mkc_os_process.h
mkc_os_process.o: include/mkc_nodiscard.h include/mkc_tmutil.h
mkc_os_win_process.o:  include/mkc_def.h
mkc_os_win_process.o: include/mkc_os_process.h include/mkc_nodiscard.h
mkc_os_win_process.o: include/mkc_string.h
mkc_osdir.o:  include/mkc_fileop.h 
mkc_osdir.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_osdir.h
mkc_osdir.o: include/mkc_string.h
mkc_parse.o: include/mkc_ast.h include/mkc_asttoken.h include/mkc_error.h
mkc_parse.o: include/mkc_nodiscard.h include/mkc_log.h 
mkc_parse.o:  include/mkc_option.h
mkc_parse.o: include/mkc_var.h include/mkc_list.h include/mkc_fileop.h
mkc_parse.o: mkc_lex.h mkc_grammar.h 
mkc_parse.o:  include/mkc_def.h 
mkc_parse.o:  include/mkc_parse.h 
mkc_parse.o: include/mkc_string.h
mkc_path.o:  include/mkc_def.h
mkc_path.o:   include/mkc_error.h
mkc_path.o: include/mkc_nodiscard.h include/mkc_fileop.h 
mkc_path.o: include/mkc_path.h include/mkc_string.h
mkc_process.o: include/mkc_asttoken.h include/mkc_check.h
mkc_process.o: include/mkc_compiler.h include/mkc_error.h
mkc_process.o: include/mkc_nodiscard.h include/mkc_log.h 
mkc_process.o: include/mkc_profile.h include/mkc_list.h include/mkc_option.h
mkc_process.o: include/mkc_var.h include/mkc_pvar.h include/mkc_context.h
mkc_process.o: include/mkc_def.h  include/mkc_env.h
mkc_process.o: include/mkc_fileop.h include/mkc_process.h include/mkc_regex.h
mkc_process.o: include/mkc_string.h include/mkc_tmutil.h
mkc_profile.o: include/mkc_compiler.h include/mkc_error.h
mkc_profile.o: include/mkc_nodiscard.h include/mkc_list.h
mkc_profile.o: include/mkc_option.h include/mkc_profile.h include/mkc_log.h
mkc_profile.o:  include/mkc_var.h include/mkc_string.h
mkc_pvar.o:   include/mkc_def.h
mkc_pvar.o:   include/mkc_env.h
mkc_pvar.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_profile.h
mkc_pvar.o: include/mkc_compiler.h include/mkc_list.h include/mkc_log.h
mkc_pvar.o:  include/mkc_option.h include/mkc_var.h
mkc_pvar.o: include/mkc_pvar.h include/mkc_string.h
mkc_string.o: include/mkc_string.h include/mkc_nodiscard.h
mkc_tmutil.o:  include/mkc_tmutil.h
mkc_var.o:   include/mkc_def.h
mkc_var.o:   include/mkc_error.h
mkc_var.o: include/mkc_nodiscard.h include/mkc_list.h include/mkc_var.h
mkc_var.o: include/mkc_log.h  include/mkc_string.h
