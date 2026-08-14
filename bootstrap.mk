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
BOOTSTRAP_PASS2 = ${MAIN_TMP}/bootstrap-pass2.txt
BOOTSTRAP_TMPDIR = ${MAIN_TMP}/bootstrap-tmpdir.txt
BOOTSTRAP_MKC_DIRS = ${MAIN_TMP}/bootstrap-mkcfiles.txt
MKC_FILES = mkc_files
MKCF_TMP = mkc_files/tmp

# base configuration

BASE_OPTFLAGS = -g -O2
BASE_CFLAGS = -Wall -Wextra -Wno-unused-parameter \
	-I. -Iinclude
BASE_LDFLAGS = -Wall -Wextra
BASE_LIBS =

INIT_DEF = -DMKC_BOOTSTRAP=1 -D_have_regex=1 -D_package_pcre=1

# standard build

CONF_CFLAGS = $(BASE_OPTFLAGS) $(BASE_CFLAGS) \
    $$($(PKGCONF) --cflags $(MKC_REGEX_PKG))
CONF_LDFLAGS = $(BASE_OPTFLAGS) $(BASE_LDFLAGS)
CONF_LIBS = $$($(PKGCONF) --libs $(MKC_REGEX_PKG))

# debug build

DBG_OPTFLAGS = -g -ggdb3 -O0
DBG_CFLAGS = $(DBG_OPTFLAGS) $(BASE_CFLAGS) \
    $$($(PKGCONF) --cflags $(MKC_REGEX_PKG))
DBG_LDFLAGS = $(DBG_OPTFLAGS) $(BASE_LDFLAGS)
DBG_LIBS = $$($(PKGCONF) --libs $(MKC_REGEX_PKG))

# sanitize build

SAN_OPTFLAGS = -g -ggdb3 -Og
#SAN_OPTFLAGS = -g -ggdb3 -O2
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
SAN_LINUX_LIBS =

WIN=F
WINOBJ=os_win_process.o

# initial targets

.PHONY: start
start:
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CONF_CFLAGS)" \
	    LDFLAGS="$(CONF_LDFLAGS)" \
	    LIBS="$(CONF_LIBS)" \
	    real-start

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

.PHONY: real-start
real-start:
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    bootstrap-final

# the initial pass
# there is no mkc_config.h file
# MKC_BOOTSTRAP must be defined
# _have_regex and _package_pcre are also defined
$(BOOTSTRAP_INIT): $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_DIRS)
	@echo "make: -- bootstrap mkc (initial)"
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS) $(INIT_DEF)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    TARGET=all oscheck
	@touch $(BOOTSTRAP_INIT)

$(BOOTSTRAP_PASS2): $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_DIRS) \
		$(BOOTSTRAP_INIT)
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    bootstrap-pass2

# create the first mkc_config.h file, clean and re-build
# then re-generate mkc_config.h with the new code.
bootstrap-pass2: mkc_config.h
	@echo "make: -- bootstrap mkc (pass2)"
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
	@touch $(BOOTSTRAP_PASS2)

# After the first pass, the mkc_config.h file has been
# created.
# any module that uses mkc_config.h will be re-compiled
bootstrap-final: $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_DIRS) \
		$(BOOTSTRAP_PASS2)
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    TARGET=all oscheck

mkc_config.h: mkc.mkc
	@./mkc --no-cache mkc.mkc

$(BOOTSTRAP_TMPDIR):
	@test -d $(MAIN_TMP) || mkdir $(MAIN_TMP)
	@touch $(BOOTSTRAP_TMPDIR)

$(BOOTSTRAP_MKC_DIRS):
	@test -d $(MKC_FILES) || mkdir $(MKC_FILES)
	@test -d $(MKCF_TMP) || mkdir $(MKCF_TMP)
	@touch $(BOOTSTRAP_MKC_DIRS)

# clean the object files and 'mkc' only
# do not clean the mkc_files/ directory
.PHONY: initialclean
initialclean:
	@rm -f $(MKCOBJECTS) mkc

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
	strutil.o \
	mkc_error.o \
	chararr.o \
	fileop.o \
	mkc_list.o \
	mkc_const.o \
	mkc_log.o \
	value.o \
	tmutil.o \
	envutil.o \
	mkc_var.o \
	mkc_compiler.o \
	os_process.o \
	$(MKC_WIN_OBJ) \
	pathutil.o \
	scopedvar.o \
	dirop.o \
	$(MKC_REGEX_OBJ) \
	comptest.o \
	dirmatch.o \
	mkc_util.o \
	toposort.o \
	asttoken.o \
	mkc_check.o \
	mkc_context.o \
	target.o \
	mkc_process.o \
	mkc_lex.o \
	mkc_ast.o \
	mkc_parse.o \
	mkc_grammar.o \
	mkc_main.o

TOPOCHKOBJ = \
	topochk.o \
	toposort.o \
	mkc_list.o \
	strutil.o \
	mkc_error.o

mkc: $(MKCOBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(MKCOBJECTS) $(LIBS)

topochk: $(TOPOCHKOBJ)
	$(CC) $(LDFLAGS) -o $@ $(TOPOCHKOBJ)

# source

mkc_grammar.h mkc_grammar.c: mkc_grammar.y
	bison -d -v -o mkc_grammar.c --defines=mkc_grammar.h mkc_grammar.y
	cat mkc_grammar.c | \
	  sed -e 's,yynerrs = 0;,yynerrs YY_ATTRIBUTE_UNUSED = 0;,' \
	  > mkc_grammar.c.n
	mv mkc_grammar.c.n mkc_grammar.c

mkc_lex.h mkc_lex.c: mkc_lex.l
	flex --header-file=mkc_lex.h -o mkc_lex.c mkc_lex.l

# objects

.c.o:
	$(CC) -c $(CFLAGS) $<

asttoken.o: asttoken.c
chararr.o: chararr.c
comptest.o: comptest.c
dirmatch.o: dirmatch.c
dirop.o: dirop.c
envutil.o: envutil.c
fileop.o: fileop.c
mkc_ast.o: mkc_ast.c
mkc_check.o: mkc_check.c
mkc_compiler.o: mkc_compiler.c
mkc_const.o: mkc_const.c
mkc_context.o: mkc_context.c
mkc_grammar.o: mkc_grammar.c
mkc_lex.o: mkc_lex.c mkc_grammar.h
mkc_list.o: mkc_list.c
mkc_log.o: mkc_log.c
mkc_main.o: mkc_main.c
mkc_parse.o: mkc_parse.c mkc_grammar.h mkc_lex.h
mkc_process.o: mkc_process.c
$(MKC_REGEX_OBJ): $(MKC_REGEX_SRC)
mkc_util.o: mkc_util.c
mkc_var.o: mkc_var.c
os_process.o: os_process.c
os_win_process.o: os_win_process.c
pathutil.o: pathutil.c
scopedvar.o: scopedvar.c
strutil.o: strutil.c
target.o: target.c
tmutil.o: tmutil.c
toposort.o: toposort.c
value.o: value.c

topochk.o: topochk.c

# DO NOT DELETE

asttoken.o: include/asttoken.h
chararr.o: include/chararr.h include/mkc_error.h include/mkc_nodiscard.h
chararr.o: include/strutil.h
comptest.o: include/alternate.h include/mkc_list.h include/mkc_error.h
comptest.o: include/mkc_nodiscard.h include/attribute.h
comptest.o: include/mkc_compiler.h include/comptest.h include/chararr.h
comptest.o: include/mkc_log.h  include/scopedvar.h
comptest.o: include/mkc_option.h include/mkc_var.h include/value.h
comptest.o: include/fileop.h include/mkc_const.h include/os_process.h
comptest.o:  include/pathutil.h include/mkc_def.h
comptest.o:  include/strutil.h
dirmatch.o:   include/dirop.h
dirmatch.o: include/mkc_error.h  include/mkc_nodiscard.h
dirmatch.o: include/mkc_list.h include/dirmatch.h include/mkc_regex.h
dirop.o:  include/mkc_def.h 
dirop.o:  include/dirop.h include/mkc_error.h
dirop.o: include/mkc_nodiscard.h include/mkc_list.h include/fileop.h
dirop.o:  include/strutil.h
envutil.o: include/envutil.h include/strutil.h include/mkc_nodiscard.h
fileop.o:   include/mkc_def.h
fileop.o: include/mkc_error.h include/mkc_nodiscard.h include/fileop.h
fileop.o: include/strutil.h
mkc_ast.o:  include/mkc_ast.h include/asttoken.h
mkc_ast.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_log.h
mkc_ast.o:  include/chararr.h include/mkc_option.h
mkc_ast.o: include/mkc_var.h include/mkc_list.h include/value.h
mkc_ast.o: include/scopedvar.h include/mkc_compiler.h include/mkc_check.h
mkc_ast.o: include/attribute.h include/alternate.h include/comptest.h
mkc_ast.o: include/mkc_context.h include/mkc_def.h 
mkc_ast.o:  include/os_process.h include/mkc_process.h
mkc_ast.o: include/strutil.h
mkc_check.o:   include/alternate.h
mkc_check.o: include/mkc_list.h  include/mkc_error.h
mkc_check.o: include/mkc_nodiscard.h include/attribute.h
mkc_check.o: include/mkc_compiler.h include/chararr.h include/mkc_check.h
mkc_check.o: include/comptest.h include/mkc_log.h 
mkc_check.o: include/scopedvar.h include/mkc_option.h include/mkc_var.h
mkc_check.o: include/value.h include/mkc_const.h include/mkc_def.h
mkc_check.o:  include/envutil.h include/fileop.h
mkc_check.o: include/os_process.h include/pathutil.h include/mkc_regex.h
mkc_check.o: include/strutil.h include/tmutil.h
mkc_compiler.o: include/mkc_compiler.h
mkc_context.o:  include/mkc_context.h
mkc_context.o: include/mkc_error.h include/mkc_nodiscard.h
mkc_error.o:   include/mkc_error.h
mkc_error.o: include/mkc_nodiscard.h include/strutil.h
mkc_grammar.o: mkc_grammar.h  
mkc_grammar.o:  include/mkc_ast.h include/asttoken.h
mkc_grammar.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_log.h
mkc_grammar.o:  include/chararr.h include/mkc_option.h
mkc_grammar.o: include/mkc_var.h include/mkc_list.h include/value.h
mkc_grammar.o: include/scopedvar.h include/mkc_compiler.h include/mkc_def.h
mkc_grammar.o:   include/fileop.h
mkc_grammar.o: include/mkc_parse.h
mkc_lex.o:   mkc_grammar.h
mkc_lex.o:  include/mkc_ast.h include/asttoken.h
mkc_lex.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_log.h
mkc_lex.o:  include/chararr.h include/mkc_option.h
mkc_lex.o: include/mkc_var.h include/mkc_list.h include/value.h
mkc_lex.o: include/scopedvar.h include/mkc_compiler.h include/mkc_def.h
mkc_lex.o:   include/fileop.h
mkc_lex.o: include/mkc_parse.h 
mkc_list.o:  include/mkc_error.h
mkc_list.o: include/mkc_nodiscard.h include/mkc_list.h include/strutil.h
mkc_log.o:   include/chararr.h
mkc_log.o: include/mkc_error.h include/mkc_nodiscard.h include/fileop.h
mkc_log.o:  include/mkc_log.h include/strutil.h
mkc_main.o:  include/mkc_ast.h include/asttoken.h
mkc_main.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_log.h
mkc_main.o:  include/chararr.h include/mkc_option.h
mkc_main.o: include/mkc_var.h include/mkc_list.h include/value.h
mkc_main.o: include/scopedvar.h include/mkc_compiler.h include/mkc_const.h
mkc_main.o: include/mkc_def.h  include/dirop.h
mkc_main.o: include/envutil.h include/fileop.h include/mkc_parse.h
mkc_main.o: include/pathutil.h include/strutil.h include/tmutil.h
mkc_parse.o: include/mkc_ast.h include/asttoken.h include/mkc_error.h
mkc_parse.o: include/mkc_nodiscard.h include/mkc_log.h 
mkc_parse.o: include/chararr.h include/mkc_option.h include/mkc_var.h
mkc_parse.o: include/mkc_list.h include/value.h include/scopedvar.h
mkc_parse.o: include/mkc_compiler.h include/fileop.h mkc_lex.h mkc_grammar.h
mkc_parse.o:   include/mkc_def.h
mkc_parse.o:   include/mkc_parse.h
mkc_parse.o:  include/strutil.h
mkc_process.o:   include/alternate.h
mkc_process.o: include/mkc_list.h include/mkc_error.h include/mkc_nodiscard.h
mkc_process.o: include/asttoken.h include/attribute.h include/mkc_compiler.h
mkc_process.o: include/chararr.h include/comptest.h include/mkc_log.h
mkc_process.o: include/scopedvar.h include/mkc_option.h include/mkc_var.h
mkc_process.o: include/value.h include/envutil.h include/fileop.h
mkc_process.o: include/mkc_check.h include/mkc_const.h include/mkc_context.h
mkc_process.o: include/mkc_def.h  include/dirmatch.h
mkc_process.o: include/mkc_regex.h include/mkc_process.h include/strutil.h
mkc_process.o: include/mkc_util.h include/pathutil.h include/target.h
mkc_process.o: include/toposort.h include/tmutil.h
mkc_regex_pcre.o:  include/mkc_def.h
mkc_regex_pcre.o:  include/mkc_error.h
mkc_regex_pcre.o: include/mkc_nodiscard.h include/mkc_regex.h
mkc_util.o:  include/mkc_util.h
mkc_var.o:  include/mkc_const.h include/mkc_def.h
mkc_var.o:   include/mkc_error.h
mkc_var.o: include/mkc_nodiscard.h include/mkc_list.h include/mkc_var.h
mkc_var.o: include/mkc_log.h  include/chararr.h
mkc_var.o: include/value.h include/strutil.h
os_process.o:  include/os_process.h
os_process.o: include/mkc_nodiscard.h include/tmutil.h 
os_win_process.o:  include/mkc_def.h
os_win_process.o:  include/fileop.h include/mkc_error.h
os_win_process.o: include/mkc_nodiscard.h include/os_process.h
os_win_process.o: include/strutil.h include/tmutil.h
pathutil.o:   include/mkc_def.h
pathutil.o:   include/mkc_error.h
pathutil.o: include/mkc_nodiscard.h include/fileop.h 
pathutil.o: include/pathutil.h include/strutil.h
scopedvar.o:  include/envutil.h include/mkc_compiler.h
scopedvar.o: include/mkc_const.h include/mkc_def.h 
scopedvar.o:  include/mkc_error.h
scopedvar.o: include/mkc_nodiscard.h include/mkc_list.h include/mkc_log.h
scopedvar.o:  include/chararr.h include/mkc_var.h
scopedvar.o: include/value.h include/scopedvar.h include/mkc_option.h
scopedvar.o: include/strutil.h
strutil.o: include/strutil.h include/mkc_nodiscard.h
target.o: include/alternate.h include/mkc_list.h 
target.o: include/mkc_error.h include/mkc_nodiscard.h include/attribute.h
target.o: include/mkc_compiler.h include/chararr.h include/comptest.h
target.o: include/mkc_log.h  include/scopedvar.h
target.o: include/mkc_option.h include/mkc_var.h include/value.h
target.o: include/dirmatch.h include/mkc_regex.h include/fileop.h
target.o: include/mkc_const.h include/mkc_def.h 
target.o:  include/mkc_util.h include/os_process.h
target.o: include/pathutil.h include/strutil.h include/target.h
target.o: include/toposort.h include/tmutil.h
tmutil.o:   include/tmutil.h
topochk.o:   include/mkc_def.h
topochk.o:   include/mkc_error.h
topochk.o: include/mkc_nodiscard.h include/toposort.h include/mkc_list.h
topochk.o: include/strutil.h
toposort.o:   include/mkc_def.h
toposort.o:   include/mkc_error.h
toposort.o: include/mkc_nodiscard.h include/mkc_list.h include/strutil.h
toposort.o: include/toposort.h
value.o: include/mkc_def.h  
value.o: include/mkc_list.h include/mkc_error.h include/mkc_nodiscard.h
value.o: include/strutil.h include/value.h 
