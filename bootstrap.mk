#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

MAKEFLAGS += --no-print-directory
MAKEFILE = bootstrap.mk
# if pkgconf is installed, there is a pkg-config symlink
PKGCONF = pkg-config

QUIET=

MKC_REGEX_PKG = libpcre2-8
MKC_REGEX_OBJ = mkc_regex_pcre.o
MKC_REGEX_SRC = mkc_regex_pcre.c

MAIN_TMP = tmp
BOOTSTRAP_PASS1 = $(MAIN_TMP)/bootstrap-pass1.txt
BOOTSTRAP_PASS2 = $(MAIN_TMP)/bootstrap-pass2.txt
BOOTSTRAP_PROCESS = $(MAIN_TMP)/bootstrap-process.txt
BOOTSTRAP_LOCAL = $(MAIN_TMP)/bootstrap-local.txt
BOOTSTRAP_TMPDIR = $(MAIN_TMP)/bootstrap-tmpdir.txt
BOOTSTRAP_MKC_DIRS = $(MAIN_TMP)/bootstrap-mkcfiles.txt

MKC_FILES = mkc_files
MKCF_TMP = $(MKC_FILES)/tmp
PROJECT = mkc
MKC_TARGET = $(MKC_FILES)/$(PROJECT)/stage/bin/$(PROJECT)
MKC_LOCAL = $(PROJECT)
MKC_CONFIG_H = "$(PROJECT)_config.h"
MKC_MKC = "$(PROJECT).mkc"

# base configuration for bootstrap

BASE_OPTFLAGS = -g -O2
BASE_CFLAGS = -Wall -Wextra -Wno-unused-parameter \
	-I. -Iinclude
BASE_LDFLAGS = -Wall -Wextra
BASE_LIBS =
BASE_PROFILE = 

INIT_DEF = -DMKC_BOOTSTRAP=1 -D_have_regex=1 -D_package_pcre=1

# standard build

CONF_CFLAGS = $(BASE_OPTFLAGS) $(BASE_CFLAGS) \
    $$($(PKGCONF) --cflags $(MKC_REGEX_PKG))
CONF_LDFLAGS = $(BASE_OPTFLAGS) $(BASE_LDFLAGS)
CONF_LIBS = $$($(PKGCONF) --libs $(MKC_REGEX_PKG))
CONF_MKC_PROFILE =

# debug build

DBG_OPTFLAGS = -g -ggdb3 -O0
DBG_CFLAGS = $(DBG_OPTFLAGS) $(BASE_CFLAGS) \
    $$($(PKGCONF) --cflags $(MKC_REGEX_PKG))
DBG_LDFLAGS = $(DBG_OPTFLAGS) $(BASE_LDFLAGS)
DBG_LIBS = $$($(PKGCONF) --libs $(MKC_REGEX_PKG))
DBG_MKC_PROFILE = debug

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
SAN_MKC_PROFILE = sanitize

# windows

WIN=F
WINOBJ=os_win_process.o

# initial targets

.PHONY: start
start:
	@echo "=== mkc: $(MKC_MKC) $(MKC_CONFIG_H)"
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CONF_CFLAGS)" \
	    LDFLAGS="$(CONF_LDFLAGS)" \
	    LIBS="$(CONF_LIBS)" \
	    MKC_PROFILE=$(CONF_MKC_PROFILE) \
	    real-start

.PHONY: sanitize
sanitize:
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(SAN_CFLAGS)" \
	    LDFLAGS="$(SAN_LDFLAGS)" \
	    LIBS="$(SAN_LIBS)" \
	    MKC_PROFILE=$(SAN_MKC_PROFILE) \
	    real-start

.PHONY: debug
debug:
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(DBG_CFLAGS)" \
	    LDFLAGS="$(DBG_LDFLAGS)" \
	    LIBS="$(DBG_LIBS)" \
	    MKC_PROFILE=$(DBG_MKC_PROFILE) \
	    real-start

.PHONY: real-start
real-start:
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    MKC_PROFILE=$(MKC_PROFILE) \
	    $(BOOTSTRAP_PROCESS) \
	    $(MKC_LOCAL)

# bootstrap process

# pass1
# the initial pass
# there is no mkc_config.h file
# MKC_BOOTSTRAP must be defined
# _have_regex and _package_pcre are also defined
$(BOOTSTRAP_PASS1): $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_DIRS)
	@echo "$(MAKEFILE): bootstrap mkc (pass1: initial)"
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS) $(INIT_DEF)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    MKC_PROFILE=$(MKC_PROFILE) \
	    WIN=$(WIN) \
	    TARGET=bootstrap oscheck
	@# at this point, an 'mkc' exists with the internal bootstrap
	@# flag set.
	@touch $(BOOTSTRAP_PASS1)

# pass2
# generate the mkc_config.h file using the 'mkc' built in 'pass1'
# MKC_BOOTSTRAP was defined in the 'pass1' version, and the mkc.mkc
# configuration will exit after 'mkc_config.h' is generated.
$(BOOTSTRAP_PASS2): $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_DIRS) \
		$(BOOTSTRAP_PASS1)
	@echo "$(MAKEFILE): bootstrap mkc (pass2: create mkc_config.h)"
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    MKC_PROFILE=$(MKC_PROFILE) \
	    WIN=$(WIN) \
	    QUIET="@" \
	    bootstrap-pass2
	@touch $(BOOTSTRAP_PASS2)
	@echo "$(MAKEFILE): bootstrap mkc (pass3: final)"
	@# make sure mkc_config.h is re-built
	@rm -f $(MKC_CONFIG_H)

# the initial mkc_config.h file is created (as a make dependency).
# but 'mkc' still has the internal bootstrap flag set.
# clean out the objects and rebuild 'mkc'.
.PHONY: bootstrap-pass2
bootstrap-pass2: $(MKC_CONFIG_H)
	@# all objects and the executable must be removed (bootstrap flag set)
	@echo "$(MAKEFILE): bootstrap mkc (pass2: rebuild)"
	@$(MAKE) -f $(MAKEFILE) basicclean
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    MKC_PROFILE=$(MKC_PROFILE) \
	    WIN=$(WIN) \
	    TARGET=bootstrap oscheck
	@# now the 'mkc' executable has no bootstrap flag

$(BOOTSTRAP_PROCESS): $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_DIRS) \
		$(BOOTSTRAP_PASS2)
	@touch $(BOOTSTRAP_PROCESS)

$(MKC_CONFIG_H): mkc.mkc
	@./mkc --no-cache --clean --prefix '' \
	    --profile $(MKC_PROFILE) $(MKC_MKC)

$(BOOTSTRAP_TMPDIR):
	@test -d $(MAIN_TMP) || mkdir $(MAIN_TMP)
	@touch $(BOOTSTRAP_TMPDIR)

$(BOOTSTRAP_MKC_DIRS):
	@test -d $(MKC_FILES) || mkdir $(MKC_FILES)
	@test -d $(MKCF_TMP) || mkdir $(MKCF_TMP)
	@touch $(BOOTSTRAP_MKC_DIRS)

# make this phony so that it always re-runs the mkc build
.PHONY: $(MKC_LOCAL)
$(MKC_LOCAL):
	@./mkc --prefix '' --profile $(MKC_PROFILE) $(MKC_MKC)
	@# the executable and objects in the current dir are no longer needed
	@$(MAKE) -f $(MAKEFILE) basicclean
	@if [ $(WIN) = T ]; then \
	  cp -pf $(MKC_FILES)/$(PROJECT)/stage/bin/mkc $(MKC_LOCAL) ; \
	else \
	  ln -sf $(MKC_FILES)/$(PROJECT)/stage/bin/mkc $(MKC_LOCAL) ; \
	fi

# clean the object files and 'mkc' only
# do not clean the mkc_files/ directory
.PHONY: basicclean
basicclean:
	@rm -f $(MKCOBJECTS) mkc mkc_grammar.output

.PHONY: oscheck
oscheck:
	@if [ `./utils/chkforwin.sh` = T ]; then \
	  $(MAKE) -f $(MAKEFILE) windows-$(TARGET) ; \
	else \
	  $(MAKE) -f $(MAKEFILE) other-$(TARGET) ; \
	fi

.PHONY: windows-bootstrap
windows-bootstrap:
	@# _WIN32 does not seem to be defined under cygwin
	@$(MAKE) -f $(MAKEFILE) \
	    CFLAGS="$(CFLAGS) \
	    -DMKC_SYS_WIN=1" \
	    MKC_WIN_OBJ="$(WINOBJ)" \
	    WIN=T \
	    $(BOOTSTRAP_LOCAL)

.PHONY: other-bootstrap
other-bootstrap:
	@$(MAKE) -f $(MAKEFILE) $(BOOTSTRAP_LOCAL)

# for the bootstrap process

# the MKCOBJECTS variable is re-generated
# be sure it is followed by a blank line
# MKCOBJECTS keep this line
MKCOBJECTS = \
	strutil.o \
	mkc_error.o \
	list.o \
	dict.o \
	chararr.o \
	fileop.o \
	value.o \
	const.o \
	mkc_log.o \
	tmutil.o \
	dictdict.o \
	envutil.o \
	var.o \
	mkc_compiler.o \
	os_process.o \
	$(MKC_WIN_OBJ) \
	pathutil.o \
	scopedvar.o \
	dirop.o \
	$(MKC_REGEX_OBJ) \
	compile.o \
	dirmatch.o \
	mkc_util.o \
	toposort.o \
	asttoken.o \
	check.o \
	context.o \
	target.o \
	process.o \
	mkc_lex.o \
	ast.o \
	parse.o \
	mkc_grammar.o \
	mkc_main.o

TOPOCHKOBJ = \
	topochk.o \
	toposort.o \
	list.o \
	strutil.o \
	mkc_error.o

# links the local objects to create a local mkc file
$(BOOTSTRAP_LOCAL): $(MKCOBJECTS)
	$(QUIET)$(CC) $(LDFLAGS) -o $(PROJECT) $(MKCOBJECTS) $(LIBS)
	@touch $(BOOTSTRAP_LOCAL)

topochk: $(TOPOCHKOBJ)
	$(QUIET)$(CC) $(LDFLAGS) -o $@ $(TOPOCHKOBJ)

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
	$(QUIET)$(CC) -c $(CFLAGS) $<

asttoken.o: asttoken.c
chararr.o: chararr.c
compile.o: compile.c
dict.o: dict.c
dictdict.o: dictdict.c
dirmatch.o: dirmatch.c
dirop.o: dirop.c
envutil.o: envutil.c
fileop.o: fileop.c
ast.o: ast.c
check.o: check.c
mkc_compiler.o: mkc_compiler.c
const.o: const.c
context.o: context.c
mkc_grammar.o: mkc_grammar.c
mkc_lex.o: mkc_lex.c mkc_grammar.h
list.o: list.c
mkc_log.o: mkc_log.c
mkc_main.o: mkc_main.c
parse.o: parse.c mkc_grammar.h mkc_lex.h
process.o: process.c
$(MKC_REGEX_OBJ): $(MKC_REGEX_SRC)
mkc_util.o: mkc_util.c
var.o: var.c
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

# the following is generated by the 'makedepend' program
# DO NOT DELETE

ast.o:   include/ast.h
ast.o: include/asttoken.h include/mkc_error.h include/mkc_nodiscard.h
ast.o: include/mkc_log.h  include/chararr.h
ast.o: include/mkc_option.h include/var.h include/list.h include/value.h
ast.o: include/dict.h include/scopedvar.h include/mkc_compiler.h
ast.o: include/check.h include/attribute.h include/alternate.h
ast.o: include/compile.h include/context.h include/mkc_def.h
ast.o:   include/os_process.h
ast.o: include/process.h include/strutil.h
asttoken.o: include/asttoken.h
chararr.o:   include/chararr.h
chararr.o: include/mkc_error.h include/mkc_nodiscard.h include/strutil.h
check.o:   include/alternate.h
check.o: include/list.h  include/mkc_error.h
check.o: include/mkc_nodiscard.h include/attribute.h include/mkc_compiler.h
check.o: include/chararr.h include/check.h include/compile.h
check.o: include/mkc_log.h  include/mkc_option.h
check.o: include/scopedvar.h include/dict.h include/var.h include/value.h
check.o: include/const.h include/mkc_def.h
check.o: include/envutil.h include/fileop.h include/os_process.h
check.o: include/pathutil.h include/mkc_regex.h include/strutil.h
check.o: include/tmutil.h
compile.o: include/alternate.h include/list.h include/mkc_error.h
compile.o: include/mkc_nodiscard.h include/attribute.h include/mkc_compiler.h
compile.o: include/compile.h include/chararr.h include/mkc_log.h
compile.o:  include/mkc_option.h include/scopedvar.h
compile.o: include/dict.h include/var.h include/value.h include/fileop.h
compile.o: include/const.h include/os_process.h
compile.o: include/pathutil.h include/mkc_def.h
compile.o: include/strutil.h
context.o: include/context.h include/mkc_error.h include/mkc_nodiscard.h
dict.o:   include/dict.h
dict.o: include/mkc_error.h include/mkc_nodiscard.h include/list.h
dict.o: include/mkc_log.h  include/chararr.h
dict.o: include/mkc_def.h
dict.o: include/strutil.h
dictdict.o:   include/dict.h
dictdict.o: include/mkc_error.h include/mkc_nodiscard.h include/list.h
dictdict.o: include/mkc_log.h  include/chararr.h
dictdict.o: include/dictdict.h include/value.h include/mkc_def.h
dirmatch.o:   include/dirop.h
dirmatch.o: include/mkc_error.h  include/mkc_nodiscard.h
dirmatch.o: include/list.h include/dirmatch.h include/mkc_regex.h
dirop.o:   include/mkc_def.h
dirop.o:   include/dirop.h
dirop.o: include/mkc_error.h include/mkc_nodiscard.h include/list.h
dirop.o: include/fileop.h  include/strutil.h
envutil.o:   include/envutil.h
envutil.o: include/strutil.h include/mkc_nodiscard.h
fileop.o:  include/mkc_def.h include/mkc_error.h
fileop.o: include/mkc_nodiscard.h include/fileop.h include/strutil.h
list.o:   include/mkc_error.h
list.o: include/mkc_nodiscard.h include/list.h include/strutil.h
mkc_compiler.o:  include/mkc_compiler.h
mkc_error.o:  include/mkc_error.h include/mkc_nodiscard.h
mkc_error.o: include/strutil.h
mkc_grammar.o: mkc_grammar.h
mkc_grammar.o: include/ast.h include/asttoken.h include/mkc_error.h
mkc_grammar.o: include/mkc_nodiscard.h include/mkc_log.h
mkc_grammar.o: include/chararr.h include/mkc_option.h include/var.h
mkc_grammar.o: include/list.h include/value.h include/dict.h
mkc_grammar.o: include/scopedvar.h include/mkc_compiler.h include/mkc_def.h
mkc_grammar.o:   include/fileop.h
mkc_grammar.o: include/parse.h
mkc_lex.o:   mkc_grammar.h
mkc_lex.o:  include/ast.h include/asttoken.h
mkc_lex.o: include/mkc_error.h include/mkc_nodiscard.h include/mkc_log.h
mkc_lex.o:  include/chararr.h include/mkc_option.h
mkc_lex.o: include/var.h include/list.h include/value.h include/dict.h
mkc_lex.o: include/scopedvar.h include/mkc_compiler.h include/mkc_def.h
mkc_lex.o:   include/fileop.h
mkc_lex.o: include/parse.h
mkc_log.o:  include/chararr.h include/mkc_error.h
mkc_log.o: include/mkc_nodiscard.h include/fileop.h include/mkc_log.h
mkc_log.o: include/strutil.h
mkc_main.o: include/ast.h include/asttoken.h include/mkc_error.h
mkc_main.o: include/mkc_nodiscard.h include/mkc_log.h
mkc_main.o: include/chararr.h include/mkc_option.h include/var.h
mkc_main.o: include/list.h include/value.h include/dict.h include/scopedvar.h
mkc_main.o: include/mkc_compiler.h include/const.h include/mkc_def.h
mkc_main.o:  include/dirop.h include/envutil.h
mkc_main.o: include/fileop.h include/parse.h include/mkc_util.h
mkc_main.o: include/pathutil.h include/strutil.h include/tmutil.h
mkc_regex_pcre.o: include/mkc_def.h  include/mkc_error.h
mkc_regex_pcre.o: include/mkc_nodiscard.h include/mkc_regex.h
mkc_util.o:  include/dirop.h include/mkc_error.h
mkc_util.o: include/mkc_nodiscard.h include/list.h include/mkc_compiler.h
mkc_util.o: include/mkc_util.h include/pathutil.h include/mkc_def.h
os_process.o: include/os_process.h include/mkc_nodiscard.h include/tmutil.h
os_win_process.o: include/mkc_def.h  include/fileop.h
os_win_process.o: include/mkc_error.h include/mkc_nodiscard.h
os_win_process.o: include/os_process.h include/strutil.h include/tmutil.h
parse.o: include/ast.h include/asttoken.h include/mkc_error.h
parse.o: include/mkc_nodiscard.h include/mkc_log.h
parse.o:  include/chararr.h include/mkc_option.h
parse.o: include/var.h include/list.h include/value.h include/dict.h
parse.o: include/scopedvar.h include/mkc_compiler.h include/fileop.h
parse.o: mkc_lex.h mkc_grammar.h
parse.o: include/mkc_def.h
parse.o: include/parse.h  include/strutil.h
pathutil.o:  include/mkc_def.h
pathutil.o:  include/mkc_error.h include/mkc_nodiscard.h
pathutil.o: include/fileop.h  include/pathutil.h
pathutil.o: include/strutil.h
process.o:  include/alternate.h include/list.h
process.o: include/mkc_error.h include/mkc_nodiscard.h include/asttoken.h
process.o: include/attribute.h include/mkc_compiler.h include/chararr.h
process.o: include/compile.h include/mkc_log.h include/mkc_option.h
process.o: include/scopedvar.h include/dict.h include/var.h include/value.h
process.o: include/dictdict.h include/envutil.h include/fileop.h
process.o: include/check.h include/const.h include/context.h
process.o: include/mkc_def.h  include/dirmatch.h
process.o: include/mkc_regex.h include/process.h include/strutil.h
process.o: include/mkc_util.h include/pathutil.h include/target.h
process.o: include/toposort.h include/tmutil.h
scopedvar.o:   include/dict.h
scopedvar.o: include/mkc_error.h include/mkc_nodiscard.h include/list.h
scopedvar.o: include/mkc_log.h  include/chararr.h
scopedvar.o: include/dictdict.h include/value.h include/envutil.h
scopedvar.o: include/mkc_compiler.h include/const.h include/mkc_def.h
scopedvar.o:   include/var.h
scopedvar.o: include/scopedvar.h include/mkc_option.h include/strutil.h
strutil.o:   include/strutil.h
strutil.o: include/mkc_nodiscard.h
target.o:   include/alternate.h
target.o: include/list.h include/mkc_error.h include/mkc_nodiscard.h
target.o: include/attribute.h include/mkc_compiler.h include/chararr.h
target.o: include/compile.h include/mkc_log.h
target.o: include/mkc_option.h include/scopedvar.h include/dict.h
target.o: include/var.h include/value.h include/dirmatch.h
target.o: include/mkc_regex.h include/dirop.h include/fileop.h
target.o: include/const.h include/mkc_def.h
target.o:  include/mkc_util.h include/os_process.h
target.o: include/pathutil.h include/strutil.h include/target.h
target.o: include/toposort.h include/tmutil.h
tmutil.o: include/tmutil.h
topochk.o:  include/mkc_def.h
topochk.o:  include/mkc_error.h include/mkc_nodiscard.h
topochk.o: include/toposort.h include/list.h include/strutil.h
toposort.o:  include/mkc_def.h
toposort.o:   include/mkc_error.h
toposort.o: include/mkc_nodiscard.h include/list.h include/strutil.h
toposort.o: include/toposort.h
value.o:   include/const.h
value.o: include/mkc_def.h
value.o: include/list.h include/mkc_error.h include/mkc_nodiscard.h
value.o: include/strutil.h include/value.h
value.o: include/dict.h include/mkc_log.h include/chararr.h
var.o:   include/const.h
var.o: include/mkc_def.h
var.o: include/mkc_error.h include/mkc_nodiscard.h include/list.h
var.o: include/var.h include/mkc_log.h  include/chararr.h
var.o: include/value.h include/dict.h include/strutil.h
