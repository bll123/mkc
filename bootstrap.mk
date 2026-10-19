#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

MAKEFLAGS += --no-print-directory
MAKEFILE = bootstrap.mk
BASICMAKEFILE = basic.mk
# if pkgconf is installed, there is a pkg-config symlink
PKGCONF = pkg-config

QUIET=
# MKC_DEBUG = -vvv
MKC_DEBUG =

MKC_REGEX_PKG = libpcre2-8

DIR_MAIN_TMP = mkc-tmp
BOOTSTRAP_TMPDIR = $(DIR_MAIN_TMP)/bootstrap-tmpdir.txt
BOOTSTRAP_MKC_DIRS = $(DIR_MAIN_TMP)/bootstrap-mkcfiles.txt

DIR_PASS1 = $(DIR_MAIN_TMP)/pass1
DIR_PASS2 = $(DIR_MAIN_TMP)/pass2
# be sure to have trailing slash
DIR_UP2 = ../../

DIR_MKCFILES = mkc_files
DIR_MKCF_TMP = $(DIR_MKCFILES)/tmp

SRC_REL =
PROJECT = mkc
MKC_TARGET = $(DIR_MKCFILES)/$(PROJECT)/stage$(PREFIX)/bin/$(PROJECT)
MKC_LOCAL = $(PROJECT)
MKC_PASS1 = $(DIR_PASS1)/$(PROJECT)
MKC_PASS2 = $(DIR_PASS2)/$(PROJECT)
MKC_CONFIG_H = "$(PROJECT)_config.h"
MKC_MKC = "$(PROJECT).mkc"
MKC_EXEC = $(MKC_LOCAL)

# base configuration for bootstrap

BASE_OPTFLAGS = -g -O2
BASE_CFLAGS = -Wall -Wextra -Wno-unused-parameter -Wno-char-subscripts
BASE_INCLUDE = -I. -Iinclude
BASE_LDFLAGS = -Wall -Wextra
BASE_LIBS =
BASE_PROFILE =

INIT_DEF = -DMKC_BOOTSTRAP=1 -D_have_regex=1 -D_package_pcre=1
INIT_INCLUDE = -I../.. -I../../include
PASS2_DEF = -DMKC_BOOTSTRAP_PASS2=1

# standard build

CONF_CFLAGS = $(BASE_OPTFLAGS) $(BASE_CFLAGS) \
    $$($(PKGCONF) --cflags $(MKC_REGEX_PKG))
CONF_LDFLAGS = $(BASE_OPTFLAGS) $(BASE_LDFLAGS)
CONF_LIBS = $$($(PKGCONF) --libs $(MKC_REGEX_PKG))
CONF_MKC_PROFILE = default

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

WIN = F
WINOBJ = os_win_process.o
EXEEXT =

# initial targets

.PHONY: start
start:
	@$(MAKE) -f $(SRC_REL)$(MAKEFILE) \
	    CFLAGS="$(CONF_CFLAGS)" \
	    LDFLAGS="$(CONF_LDFLAGS)" \
	    LIBS="$(CONF_LIBS)" \
	    MKC_PROFILE=$(CONF_MKC_PROFILE) \
	    real-start

.PHONY: sanitize
sanitize:
	@$(MAKE) -f $(SRC_REL)$(MAKEFILE) \
	    CFLAGS="$(SAN_CFLAGS)" \
	    LDFLAGS="$(SAN_LDFLAGS)" \
	    LIBS="$(SAN_LIBS)" \
	    MKC_PROFILE=$(SAN_MKC_PROFILE) \
	    real-start

.PHONY: debug
debug:
	@$(MAKE) -f $(SRC_REL)$(MAKEFILE) \
	    CFLAGS="$(DBG_CFLAGS)" \
	    LDFLAGS="$(DBG_LDFLAGS)" \
	    LIBS="$(DBG_LIBS)" \
	    MKC_PROFILE=$(DBG_MKC_PROFILE) \
	    real-start

.PHONY: real-start
real-start:
	@$(MAKE) -f $(SRC_REL)$(MAKEFILE) \
	    CFLAGS="$(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS)" \
	    LIBS="$(LIBS)" \
	    MKC_PROFILE=$(MKC_PROFILE) \
	    TARGET=bootstrap \
            oscheck

# source

SOURCE_FILES = mkc_grammar.h mkc_grammar.c mkc_lex.h mkc_lex.c

# bootstrap process

# .PHONY: bootstrap-init
bootstrap-init: $(BOOTSTRAP_TMPDIR) $(BOOTSTRAP_MKC_DIRS)
	@:

# pass1
# the initial pass
# there is no mkc_config.h file
# MKC_BOOTSTRAP must be defined
# _have_regex and _package_pcre are also defined
# .PHONY: bootstrap-pass1
bootstrap-pass1: $(SOURCE_FILES) bootstrap-init
	@echo "$(MAKEFILE): bootstrap mkc (pass1: initial)"
	$(MAKE) -C $(DIR_PASS1) -f $(DIR_UP2)$(BASICMAKEFILE) \
	    CFLAGS="$(CFLAGS) $(INIT_INCLUDE) $(INIT_DEF)" \
            SRC_REL=$(DIR_UP2) \
            VPATH=$(DIR_UP2) \
	    WIN=$(WIN) \
	    $(MKC_LOCAL)
	@# at this point, 'mkc-tmp/pass1/mkc' exists with the internal
	@# bootstrap flag set.

# pass2
# generate the mkc_config.h file using the 'mkc' built in 'pass1'
# MKC_BOOTSTRAP was defined in the 'pass1' version, and the mkc.mkc
# configuration will exit after 'mkc_config.h' is generated.
bootstrap-pass2: bootstrap-pass1
	@echo "$(MAKEFILE): bootstrap mkc (pass2: create mkc_config.h)"
	@$(MAKE) -f $(MAKEFILE) \
	    MKC_EXEC=$(MKC_PASS1) \
	    bootstrap-config
	$(MAKE) -C $(DIR_PASS2) -f $(DIR_UP2)$(BASICMAKEFILE) \
	    CFLAGS="$(CFLAGS) $(INIT_INCLUDE) $(PASS2_DEF)" \
	    QUIET="@" \
	    SRC_REL=$(DIR_UP2) \
	    VPATH=$(DIR_UP2) \
	    WIN=$(WIN) \
	    $(MKC_LOCAL)
	@# 'mkc-tmp/pass2/mkc' now exists.  no bootstrap flag is set

bootstrap-config: mkc_config.h
	@:

$(MKC_TARGET): bootstrap-pass2
	@$(MAKE) -f $(MAKEFILE) \
	    MKC_EXEC=$(MKC_PASS2) \
	    WIN=$(WIN) \
	    bootstrap-process

.PHONY: bootstrap-process
bootstrap-process:
	@./$(MKC_EXEC) --prefix '' \
	    --profile $(MKC_PROFILE) $(MKC_DEBUG) $(MKC_MKC)

$(MKC_LOCAL): $(MKC_TARGET)
	@if [ $(WIN) = T ]; then \
	  cp -pf $(MKC_TARGET) $(MKC_LOCAL) ; \
	else \
	  ln -sf $(MKC_TARGET) $(MKC_LOCAL) ; \
	fi

# if the bootstrap flag is set in the executable, mkc.mkc
# will only build the configuration file.
# if the bootstrap flag is not set,
# a complete build is performed using mkc.mkc.
mkc_config.h: mkc.mkc
	$(MKC_EXEC) --no-cache --clean --prefix '' \
	    --profile $(MKC_PROFILE) $(MKC_DEBUG) $(MKC_MKC)

$(BOOTSTRAP_TMPDIR):
	@test -d $(DIR_MAIN_TMP) || mkdir $(DIR_MAIN_TMP)
	@test -d $(DIR_PASS1) || mkdir $(DIR_PASS1)
	@test -d $(DIR_PASS2) || mkdir $(DIR_PASS2)
	@touch $(BOOTSTRAP_TMPDIR)

$(BOOTSTRAP_MKC_DIRS):
	@test -d $(DIR_MKCFILES) || mkdir $(DIR_MKCFILES)
	@test -d $(DIR_MKCF_TMP) || mkdir $(DIR_MKCF_TMP)
	@touch $(BOOTSTRAP_MKC_DIRS)

.PHONY: oscheck
oscheck:
	if [ `$(SRC_REL)utils/chkforwin.sh` = T ]; then \
	  $(MAKE) -f $(SRC_REL)$(MAKEFILE) windows-$(TARGET) ; \
	else \
	  $(MAKE) -f $(SRC_REL)$(MAKEFILE) other-$(TARGET) ; \
	fi

.PHONY: windows-bootstrap
windows-bootstrap:
	@$(MAKE) -f $(SRC_REL)$(MAKEFILE) \
	    CFLAGS="$(CFLAGS) \
	      -DMKC_BOOTSTRAP_WIN=1" \
	    MKC_WIN_OBJ="$(WINOBJ)" \
	    WIN=T \
	    EXEEXT=".exe" \
	    $(MKC_LOCAL)

.PHONY: other-bootstrap
other-bootstrap:
	@$(MAKE) -f $(SRC_REL)$(MAKEFILE) $(MKC_LOCAL)

# source

# -d
mkc_grammar.h mkc_grammar.c: mkc_grammar.y
	bison -v -o mkc_grammar.c \
	    --defines=mkc_grammar.h mkc_grammar.y
	sed -e 's,yynerrs = 0;,yynerrs YY_ATTRIBUTE_UNUSED = 0;,' \
	  mkc_grammar.c \
	  > mkc_grammar.c.n
	mv mkc_grammar.c.n mkc_grammar.c

mkc_lex.h mkc_lex.c: mkc_lex.l
	flex --header-file=mkc_lex.h \
	    -o mkc_lex.c mkc_lex.l

