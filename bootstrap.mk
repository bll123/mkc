#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

MAKEFLAGS += --no-print-directory
MAKEFILE = bootstrap.mk

CFLAGS = -g -ggdb3 -Og -Wall -Wextra -Wno-unused-parameter \
	-DMKC_BOOTSTRAP=1 -Iinclude
LDFLAGS = -g -ggdb3 -Og -Wall -Wextra
LIBS =

SANCFLAGS = -g -ggdb3 -Og -Wall -Wextra -DMKC_BOOTSTRAP=1 -Iinclude \
    -Wno-unused-parameter \
    -fsanitize=address,undefined \
    -fsanitize-address-use-after-scope \
    -fsanitize-recover=address \
    -fno-omit-frame-pointer \
    -fno-common \
    -fno-inline
SANLDFLAGS = -g \
    -fsanitize=address,undefined \
    -fsanitize-address-use-after-scope \
    -fsanitize-recover=address \
    -lrt

CONFCFLAGS = -g -ggdb3 -Og -Wall -Wextra -Wno-unused-parameter \
	-Iinclude $$(pkgconf --cflags libpkgconf)
CONFLDFLAGS = -g -ggdb3 -Og -Wall -Wextra
CONFLIBS = $$(pkgconf --libs libpkgconf)

WIN=F
WINOBJ=mkc_os_win_process.o

.PHONY: all
all:
	@if [ -f mkc_config.h ]; then \
	  echo "-- bootstrap mkc (mkc_config.h)"; \
	  if [ -f bootstrap.txt ]; then \
	    $(MAKE) clean; \
	    rm -f bootstrap.txt; \
	  fi; \
	  $(MAKE) -f $(MAKEFILE) \
	      CFLAGS="$(CONFCFLAGS)" LDFLAGS="$(CONFLDFLAGS)" \
	      LIBS="$(CONFLIBS)" TARGET=$@ oscheck; \
	else \
	  echo "-- bootstrap mkc"; \
	  $(MAKE) -f $(MAKEFILE) TARGET=$@ oscheck; \
          if [ $$? -eq 0 ]; then \
	    ./mkc mkc.mkc; \
	    touch bootstrap.txt; \
	  fi; \
	fi

.PHONY: sanitize
sanitize:
	$(MAKE) -f $(MAKEFILE) TARGET=all \
	    CFLAGS="$(SANCFLAGS)" LDFLAGS="$(SANLDFLAGS)" oscheck

.PHONY: oscheck
oscheck:
	@if [ `./utils/chkforwin.sh` = T ]; then \
	  $(MAKE) -f $(MAKEFILE) windows-$(TARGET) ; \
	else \
	  $(MAKE) -f $(MAKEFILE) other-$(TARGET) ; \
	fi

.PHONY: windows-all
windows-all:
	@$(MAKE) -f $(MAKEFILE) MKCOBJOSWIN="$(WINOBJ)" real-all

.PHONY: other-all
other-all:
	@$(MAKE) -f $(MAKEFILE) real-all

.PHONY: real-all
real-all: mkc

MKCOBJECTS = mkc_main.o \
	mkc_parse.o \
	mkc_lex.o mkc_grammar.o \
	mkc_ast.o mkc_check.o \
	mkc_process.o \
	mkc_pvar.o mkc_profile.o mkc_var.o mkc_list.o \
	mkc_context.o mkc_asttoken.o \
	mkc_os_process.o $(MKCOBJOSWIN) \
	mkc_compiler.o \
	mkc_log.o mkc_string.o mkc_fileop.o \
	mkc_error.o mkc_env.o mkc_tmutil.o

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
mkc_context.o: mkc_check.c
mkc_env.o: mkc_env.c
mkc_fileop.o: mkc_fileop.c
mkc_grammar.o: mkc_grammar.c
mkc_lex.o: mkc_lex.c mkc_grammar.h
mkc_list.o: mkc_list.c
mkc_log.o: mkc_log.c
mkc_main.o: mkc_main.c
mkc_os_process.o: mkc_os_process.c
mkc_os_win_process.o: mkc_os_winprocess.c
mkc_parse.o: mkc_parse.c mkc_grammar.h mkc_lex.h
mkc_process.o: mkc_process.c
mkc_profile.o: mkc_profile.c
mkc_pvar.o: mkc_pvar.c
mkc_string.o: mkc_string.c
mkc_tmutil.o: mkc_tmutil.c
mkc_var.o: mkc_var.c

# DO NOT DELETE

mkc_ast.o:   include/mkc_ast.h
mkc_ast.o: include/mkc_asttoken.h include/mkc_error.h include/mkc_log.h
mkc_ast.o:  include/mkc_option.h include/mkc_var.h
mkc_ast.o: include/mkc_list.h include/mkc_check.h include/mkc_compiler.h
mkc_ast.o: include/mkc_profile.h include/mkc_pvar.h include/mkc_context.h
mkc_ast.o: include/mkc_def.h  
mkc_ast.o: include/mkc_os_process.h include/mkc_process.h
mkc_ast.o: include/mkc_string.h include/mkc_nodiscard.h
mkc_asttoken.o: include/mkc_asttoken.h
mkc_check.o: include/mkc_check.h include/mkc_compiler.h include/mkc_error.h
mkc_check.o:  include/mkc_log.h 
mkc_check.o: include/mkc_profile.h include/mkc_list.h include/mkc_option.h
mkc_check.o: include/mkc_var.h include/mkc_pvar.h include/mkc_def.h
mkc_check.o:   include/mkc_env.h
mkc_check.o: include/mkc_fileop.h include/mkc_nodiscard.h
mkc_check.o: include/mkc_os_process.h include/mkc_string.h
mkc_compiler.o:  include/mkc_compiler.h
mkc_context.o:  include/mkc_context.h
mkc_context.o: include/mkc_error.h
mkc_env.o:  include/mkc_env.h include/mkc_string.h
mkc_env.o: include/mkc_nodiscard.h
mkc_error.o:  include/mkc_error.h include/mkc_string.h
mkc_error.o: include/mkc_nodiscard.h
mkc_fileop.o:   include/mkc_def.h
mkc_fileop.o: include/mkc_error.h include/mkc_fileop.h
mkc_fileop.o: include/mkc_nodiscard.h include/mkc_string.h
mkc_list.o:   include/mkc_error.h
mkc_list.o: include/mkc_list.h include/mkc_string.h include/mkc_nodiscard.h
mkc_log.o: include/mkc_error.h include/mkc_fileop.h 
mkc_log.o: include/mkc_nodiscard.h include/mkc_log.h
mkc_main.o:  include/mkc_ast.h include/mkc_asttoken.h
mkc_main.o: include/mkc_error.h include/mkc_log.h 
mkc_main.o: include/mkc_option.h include/mkc_var.h include/mkc_list.h
mkc_main.o: include/mkc_fileop.h include/mkc_nodiscard.h include/mkc_parse.h
mkc_main.o: include/mkc_profile.h include/mkc_compiler.h include/mkc_string.h
mkc_main.o: include/mkc_tmutil.h
mkc_os_process.o:  include/mkc_os_process.h
mkc_os_process.o: include/mkc_tmutil.h 
mkc_os_win_process.o:  include/mkc_def.h
mkc_os_win_process.o: include/mkc_os_process.h include/mkc_string.h
mkc_os_win_process.o: include/mkc_nodiscard.h
mkc_parse.o: include/mkc_ast.h include/mkc_asttoken.h include/mkc_error.h
mkc_parse.o: include/mkc_log.h  
mkc_parse.o: include/mkc_option.h include/mkc_var.h include/mkc_list.h
mkc_parse.o: include/mkc_fileop.h include/mkc_nodiscard.h include/mkc_parse.h
mkc_process.o: include/mkc_asttoken.h include/mkc_check.h
mkc_process.o: include/mkc_compiler.h include/mkc_error.h include/mkc_log.h
mkc_process.o:  include/mkc_profile.h include/mkc_list.h
mkc_process.o: include/mkc_option.h include/mkc_var.h include/mkc_pvar.h
mkc_process.o: include/mkc_context.h include/mkc_def.h 
mkc_process.o: include/mkc_env.h include/mkc_fileop.h include/mkc_nodiscard.h
mkc_process.o: include/mkc_process.h include/mkc_string.h
mkc_process.o: include/mkc_tmutil.h
mkc_profile.o: include/mkc_compiler.h include/mkc_error.h include/mkc_list.h
mkc_profile.o: include/mkc_option.h include/mkc_profile.h include/mkc_log.h
mkc_profile.o:  include/mkc_var.h include/mkc_string.h
mkc_profile.o: include/mkc_nodiscard.h
mkc_pvar.o:   include/mkc_def.h
mkc_pvar.o:   include/mkc_env.h
mkc_pvar.o: include/mkc_error.h include/mkc_profile.h include/mkc_compiler.h
mkc_pvar.o: include/mkc_list.h include/mkc_log.h 
mkc_pvar.o: include/mkc_option.h include/mkc_var.h include/mkc_pvar.h
mkc_pvar.o: include/mkc_string.h include/mkc_nodiscard.h
mkc_string.o: include/mkc_string.h include/mkc_nodiscard.h
mkc_tmutil.o:  include/mkc_tmutil.h
mkc_var.o:   include/mkc_def.h
mkc_var.o:   include/mkc_error.h
mkc_var.o: include/mkc_list.h include/mkc_var.h include/mkc_log.h
mkc_var.o:  include/mkc_string.h include/mkc_nodiscard.h
