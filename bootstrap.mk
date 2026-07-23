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
SAN_LIBS = $$(pkgconf --libs $(MKC_REGEX_PKG))
SAN_LINUX_LIBS = -lrt


WIN=F
WINOBJ=os_win_process.o

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
	@if [ `uname -s` = Linux ]; then \
	  $(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(SAN_CFLAGS)" \
	    LDFLAGS="$(SAN_LDFLAGS)" \
	    LIBS="$(SAN_LIBS) $(SAN_LINUX_LIBS)" \
	    real-start ; \
	else \
	  $(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(SAN_CFLAGS)" \
	    LDFLAGS="$(SAN_LDFLAGS)" \
	    LIBS="$(SAN_LIBS)" \
	    real-start ; \
	fi

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

# topochk
.PHONY: real-all
real-all: mkc

# the MKCOBJECTS variable is re-generated
# be sure it is followed by a blank line
# MKCOBJECTS keep this line
MKCOBJECTS = \
	mkc_main.o \
	mkc_grammar.o \
	mkc_parse.o \
	mkc_ast.o \
	mkc_lex.o \
	scope.o \
	mkc_process.o \
	toposort.o \
	mkc_util.o \
	mkc_dirmatch.o \
	mkc_context.o \
	mkc_check.o \
	asttoken.o \
	dirop.o \
	pathutil.o \
	os_process.o \
	$(MKC_REGEX_OBJ) \
	mkc_pvar.o \
	tmutil.o \
	mkc_profile.o \
	envutil.o \
	mkc_var.o \
	mkc_compiler.o \
	value.o \
	mkc_log.o \
	mkc_const.o \
	mkc_list.o \
	fileop.o \
	mkc_error.o \
	strutil.o

TOPOCHKOBJ = \
	topochk.o \
	toposort.o \
	mkc_list.o \
	strutil.o \
	mkc_error.o

# the INITIALOBJ variable will be replaced
# be sure it is followed by a blank line
# INITIALOBJ keep this line
INITIALOBJ = \
	mkc_grammar.o \
	dirop.o \
	envutil.o \
	fileop.o \
	mkc_check.o \
	mkc_dirmatch.o \
	mkc_main.o \
	mkc_process.o \
	mkc_regex_pcre.o \
	os_process.o \
	os_win_process.o \
	pathutil.o \
	strutil.o \
	tmutil.o

# the NOREGEXOBJ variable will be replaced
# be sure it is followed by a blank line
# NOREGEXOBJ keep this line
NOREGEXOBJ = \
	dirop.o \
	mkc_main.o

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

asttoken.o: asttoken.c
dirop.o: dirop.c
envutil.o: envutil.c
fileop.o: fileop.c
mkc_ast.o: mkc_ast.c
mkc_check.o: mkc_check.c
mkc_compiler.o: mkc_compiler.c
mkc_const.o: mkc_const.c
mkc_context.o: mkc_context.c
mkc_dirmatch.o: mkc_dirmatch.c
mkc_grammar.o: mkc_grammar.c
mkc_lex.o: mkc_lex.c mkc_grammar.h
mkc_list.o: mkc_list.c
mkc_log.o: mkc_log.c
mkc_main.o: mkc_main.c
mkc_parse.o: mkc_parse.c mkc_grammar.h mkc_lex.h
mkc_process.o: mkc_process.c
mkc_profile.o: mkc_profile.c
mkc_pvar.o: mkc_pvar.c
$(MKC_REGEX_OBJ): $(MKC_REGEX_SRC)
mkc_util.o: mkc_util.c
mkc_var.o: mkc_var.c
os_process.o: os_process.c
os_win_process.o: os_win_process.c
pathutil.o: pathutil.c
scope.o: scope.c
strutil.o: strutil.c
tmutil.o: tmutil.c
toposort.o: toposort.c
value.o: value.c

topochk.o: topochk.c

# DO NOT DELETE

asttoken.o: include/asttoken.h
dirop.o:  include/mkc_def.h 
dirop.o:  include/dirop.h include/mkc_error.h
dirop.o: include/mkc_nodiscard.h include/mkc_list.h include/fileop.h
dirop.o:  include/strutil.h
envutil.o:   include/envutil.h
envutil.o: include/strutil.h include/mkc_nodiscard.h
fileop.o: include/mkc_def.h include/mkc_error.h include/mkc_nodiscard.h
fileop.o: include/fileop.h include/strutil.h
mkc_ast.o:  include/mkc_ast.h include/asttoken.h
mkc_ast.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_log.h
mkc_ast.o:  include/mkc_option.h include/mkc_var.h
mkc_ast.o: include/mkc_list.h include/value.h include/scope.h
mkc_ast.o: include/mkc_check.h include/mkc_compiler.h include/mkc_profile.h
mkc_ast.o: include/mkc_pvar.h include/mkc_context.h include/mkc_def.h
mkc_ast.o:   include/os_process.h
mkc_ast.o: include/mkc_process.h include/strutil.h
mkc_check.o:  include/mkc_check.h
mkc_check.o: include/mkc_compiler.h include/mkc_error.h 
mkc_check.o: include/mkc_nodiscard.h include/mkc_list.h include/mkc_log.h
mkc_check.o:  include/mkc_profile.h include/mkc_option.h
mkc_check.o: include/mkc_var.h include/value.h include/mkc_pvar.h
mkc_check.o: include/scope.h include/mkc_const.h include/mkc_def.h
mkc_check.o:   include/envutil.h
mkc_check.o: include/fileop.h include/os_process.h include/pathutil.h
mkc_check.o: include/mkc_regex.h include/strutil.h
mkc_compiler.o:  include/mkc_compiler.h
mkc_context.o:  include/mkc_context.h
mkc_context.o: include/mkc_error.h include/mkc_nodiscard.h
mkc_dirmatch.o:  include/dirop.h include/mkc_error.h
mkc_dirmatch.o:  include/mkc_nodiscard.h
mkc_dirmatch.o: include/mkc_list.h include/mkc_dirmatch.h include/mkc_regex.h
mkc_error.o:   include/mkc_error.h
mkc_error.o: include/mkc_nodiscard.h include/strutil.h
mkc_grammar.o: mkc_grammar.h  
mkc_grammar.o:   include/mkc_ast.h
mkc_grammar.o: include/asttoken.h include/mkc_error.h include/mkc_nodiscard.h
mkc_grammar.o: include/mkc_log.h  
mkc_grammar.o: include/mkc_option.h include/mkc_var.h include/mkc_list.h
mkc_grammar.o: include/value.h include/scope.h include/mkc_def.h
mkc_grammar.o:   include/fileop.h
mkc_grammar.o: include/mkc_parse.h
mkc_lex.o:   mkc_grammar.h
mkc_lex.o:   include/mkc_ast.h
mkc_lex.o: include/asttoken.h include/mkc_error.h include/mkc_nodiscard.h
mkc_lex.o: include/mkc_log.h  
mkc_lex.o: include/mkc_option.h include/mkc_var.h include/mkc_list.h
mkc_lex.o: include/value.h include/scope.h include/mkc_def.h
mkc_lex.o:   include/fileop.h
mkc_lex.o: include/mkc_parse.h 
mkc_list.o:  include/mkc_error.h include/mkc_nodiscard.h
mkc_list.o: include/mkc_list.h include/strutil.h
mkc_log.o:   include/mkc_error.h
mkc_log.o: include/mkc_nodiscard.h include/fileop.h 
mkc_log.o: include/mkc_log.h include/strutil.h
mkc_main.o:  include/mkc_ast.h include/asttoken.h
mkc_main.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_log.h
mkc_main.o:  include/mkc_option.h include/mkc_var.h
mkc_main.o: include/mkc_list.h include/value.h include/scope.h
mkc_main.o: include/mkc_const.h include/mkc_def.h 
mkc_main.o: include/dirop.h include/envutil.h include/fileop.h
mkc_main.o: include/mkc_parse.h include/pathutil.h include/mkc_profile.h
mkc_main.o: include/mkc_compiler.h include/strutil.h include/tmutil.h
mkc_parse.o: include/mkc_ast.h include/asttoken.h include/mkc_error.h
mkc_parse.o: include/mkc_nodiscard.h include/mkc_log.h 
mkc_parse.o:  include/mkc_option.h
mkc_parse.o: include/mkc_var.h include/mkc_list.h include/value.h
mkc_parse.o: include/scope.h include/fileop.h mkc_lex.h mkc_grammar.h
mkc_parse.o:  include/mkc_def.h 
mkc_parse.o:  include/mkc_parse.h 
mkc_parse.o: include/strutil.h
mkc_process.o:   include/asttoken.h
mkc_process.o: include/envutil.h include/fileop.h include/mkc_error.h
mkc_process.o: include/mkc_nodiscard.h include/mkc_check.h
mkc_process.o: include/mkc_compiler.h include/mkc_list.h include/mkc_log.h
mkc_process.o: include/mkc_profile.h include/mkc_option.h include/mkc_var.h
mkc_process.o: include/value.h include/mkc_pvar.h include/scope.h
mkc_process.o: include/mkc_const.h include/mkc_context.h include/mkc_def.h
mkc_process.o:  include/mkc_dirmatch.h
mkc_process.o: include/mkc_regex.h include/mkc_process.h include/strutil.h
mkc_process.o: include/mkc_util.h include/pathutil.h include/tmutil.h
mkc_process.o: include/toposort.h
mkc_profile.o: include/mkc_compiler.h include/mkc_const.h include/mkc_error.h
mkc_profile.o: include/mkc_nodiscard.h include/mkc_list.h
mkc_profile.o: include/mkc_option.h include/mkc_profile.h include/mkc_log.h
mkc_profile.o:  include/mkc_var.h include/value.h
mkc_profile.o: include/strutil.h
mkc_pvar.o:  include/mkc_const.h include/mkc_def.h
mkc_pvar.o:   include/envutil.h
mkc_pvar.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_profile.h
mkc_pvar.o: include/mkc_compiler.h include/mkc_list.h include/mkc_log.h
mkc_pvar.o:  include/mkc_option.h include/mkc_var.h
mkc_pvar.o: include/value.h include/mkc_pvar.h include/scope.h
mkc_pvar.o: include/strutil.h
mkc_regex_pcre.o:  include/mkc_def.h
mkc_regex_pcre.o:  include/mkc_error.h
mkc_regex_pcre.o: include/mkc_nodiscard.h include/mkc_regex.h
mkc_var.o:  include/mkc_const.h include/mkc_def.h
mkc_var.o:   include/mkc_error.h
mkc_var.o: include/mkc_nodiscard.h include/mkc_list.h include/mkc_var.h
mkc_var.o: include/mkc_log.h  include/value.h
mkc_var.o: include/strutil.h
os_process.o: include/os_process.h include/mkc_nodiscard.h include/tmutil.h
os_win_process.o:  include/mkc_def.h
os_win_process.o:  include/fileop.h include/mkc_error.h
os_win_process.o: include/mkc_nodiscard.h include/os_process.h
os_win_process.o: include/strutil.h include/tmutil.h
pathutil.o:   include/mkc_def.h
pathutil.o:  include/mkc_error.h include/mkc_nodiscard.h
pathutil.o: include/fileop.h  include/pathutil.h
pathutil.o: include/strutil.h
scope.o:  include/mkc_compiler.h include/mkc_error.h
scope.o: include/mkc_nodiscard.h include/mkc_list.h include/mkc_log.h
scope.o:  include/mkc_var.h include/value.h
scope.o: include/scope.h
strutil.o: include/strutil.h include/mkc_nodiscard.h
tmutil.o: include/tmutil.h
topochk.o:  include/mkc_def.h 
topochk.o: include/mkc_error.h include/mkc_nodiscard.h include/toposort.h
topochk.o: include/mkc_list.h include/strutil.h
toposort.o:  include/mkc_def.h 
toposort.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_list.h
toposort.o: include/strutil.h include/toposort.h
value.o:  include/mkc_def.h 
value.o:  include/mkc_list.h include/mkc_error.h
value.o: include/mkc_nodiscard.h include/strutil.h
value.o: include/value.h 
