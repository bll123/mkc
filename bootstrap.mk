#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

MAKEFLAGS += --no-print-directory
MAKEFILE = bootstrap.mk

CFLAGS = -g -ggdb3 -Og -Wall -Wextra -Wno-unused-parameter -DMKC_BOOTSTRAP=1
SANCFLAGS = -g -ggdb3 -Og -Wall -Wextra -DMKC_BOOTSTRAP=1 \
    -Wno-unused-parameter \
    -fsanitize=address,undefined \
    -fsanitize-address-use-after-scope \
    -fsanitize-recover=address \
    -fno-omit-frame-pointer \
    -fno-common \
    -fno-inline
LDFLAGS = -g -ggdb3 -Og -Wall -Wextra
SANLDFLAGS = -g \
    -fsanitize=address,undefined \
    -fsanitize-address-use-after-scope \
    -fsanitize-recover=address \
    -lrt

WIN=F
WINOBJ=mkc_os_win_process.o

.PHONY: all
all:
	@$(MAKE) -f $(MAKEFILE) TARGET=$@ oscheck

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
	@$(MAKE) -f $(MAKEFILE) OBJOSWIN="$(WINOBJ)" real-all

.PHONY: other-all
other-all:
	@$(MAKE) -f $(MAKEFILE) real-all

.PHONY: real-all
real-all: mkc

OBJECTS = mkc_main.o \
	mkc_lex.o mkc_grammar.o \
	mkc_ast.o mkc_check.o \
	mkc_process.o \
	mkc_pvar.o mkc_profile.o mkc_var.o mkc_list.c \
	mkc_context.o \
	mkc_parse_util.o mkc_os_process.o $(OBJOSWIN) \
	mkc_log.o mkc_string.o mkc_fileop.o \
	mkc_error.o mkc_env.o mkc_util.o

mkc: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

# source

mkc_grammar.h mkc_grammar.c: mkc_grammar.y
	bison -d -v -o mkc_grammar.c --defines=mkc_grammar.h mkc_grammar.y

mkc_lex.h mkc_lex.c: mkc_lex.l
	flex --header-file=mkc_lex.h -o mkc_lex.c mkc_lex.l

# objects

.c.o:
	$(CC) -c $(CFLAGS) $<

mkc_ast.o: mkc_ast.c
mkc_check.o: mkc_check.c
mkc_context.o: mkc_check.c
mkc_env.o: mkc_env.c
mkc_fileop.o: mkc_fileop.c
mkc_grammar.o: mkc_grammar.c
mkc_lex.o: mkc_lex.c mkc_grammar.h
mkc_list.o: mkc_list.c
mkc_log.o: mkc_log.c
mkc_main.o: mkc_main.c mkc_grammar.h mkc_lex.h
mkc_os_process.o: mkc_os_process.c
mkc_os_win_process.o: mkc_os_winprocess.c
mkc_parse_util.o: mkc_parse_util.c
mkc_process.o: mkc_process.c
mkc_profile.o: mkc_profile.c
mkc_pvar.o: mkc_pvar.c
mkc_string.o: mkc_string.c
mkc_util.o: mkc_util.c
mkc_var.o: mkc_var.c

# DO NOT DELETE

mkc_ast.o:   mkc_ast.h mkc_error.h
mkc_ast.o: mkc_log.h  mkc_var.h mkc_list.h mkc_check.h
mkc_ast.o: mkc_env.h mkc_profile.h mkc_pvar.h mkc_context.h mkc_def.h
mkc_ast.o:   mkc_os_process.h
mkc_ast.o: mkc_parse_util.h mkc_process.h mkc_string.h mkc_nodiscard.h
mkc_check.o: mkc_check.h mkc_error.h  mkc_env.h
mkc_check.o: mkc_log.h  mkc_profile.h mkc_list.h mkc_var.h
mkc_check.o: mkc_pvar.h mkc_def.h  
mkc_check.o: mkc_fileop.h mkc_nodiscard.h mkc_os_process.h mkc_string.h
mkc_context.o:   mkc_context.h
mkc_context.o: mkc_error.h
mkc_env.o:  mkc_env.h mkc_string.h mkc_nodiscard.h
mkc_error.o:   mkc_error.h
mkc_fileop.o:  mkc_def.h mkc_error.h mkc_fileop.h
mkc_fileop.o: mkc_nodiscard.h mkc_string.h
mkc_grammar.o: mkc_grammar.h  
mkc_grammar.o: mkc_ast.h mkc_error.h mkc_log.h  mkc_var.h
mkc_grammar.o: mkc_list.h mkc_def.h 
mkc_grammar.o:  mkc_parse_util.h
mkc_lex.o:   mkc_grammar.h
mkc_lex.o:  mkc_ast.h mkc_error.h mkc_log.h
mkc_lex.o:  mkc_var.h mkc_list.h mkc_def.h
mkc_lex.o:   mkc_parse_util.h
mkc_list.o:   mkc_error.h
mkc_list.o: mkc_list.h
mkc_log.o:   mkc_error.h
mkc_log.o: mkc_fileop.h  mkc_nodiscard.h mkc_log.h
mkc_main.o:  mkc_ast.h mkc_error.h mkc_log.h
mkc_main.o:  mkc_var.h mkc_list.h mkc_fileop.h
mkc_main.o: mkc_nodiscard.h mkc_grammar.h mkc_def.h 
mkc_main.o:  mkc_parse_util.h mkc_lex.h mkc_profile.h
mkc_main.o: mkc_util.h mkc_string.h
mkc_os_process.o:  mkc_os_process.h mkc_util.h
mkc_os_win_process.o:  mkc_def.h 
mkc_os_win_process.o:  mkc_os_process.h mkc_string.h
mkc_os_win_process.o: mkc_nodiscard.h mkc_util.h
mkc_parse_util.o:  mkc_parse_util.h
mkc_process.o:  mkc_ast.h mkc_error.h mkc_log.h
mkc_process.o:  mkc_var.h mkc_list.h mkc_check.h mkc_env.h
mkc_process.o: mkc_profile.h mkc_pvar.h mkc_def.h 
mkc_process.o: mkc_fileop.h mkc_nodiscard.h mkc_process.h mkc_string.h
mkc_process.o: mkc_util.h
mkc_profile.o: mkc_error.h mkc_list.h mkc_profile.h mkc_log.h
mkc_profile.o:  mkc_var.h mkc_string.h mkc_nodiscard.h
mkc_pvar.o:   mkc_def.h
mkc_pvar.o:   mkc_env.h mkc_error.h
mkc_pvar.o: mkc_profile.h mkc_list.h mkc_log.h  mkc_var.h
mkc_pvar.o: mkc_pvar.h mkc_string.h mkc_nodiscard.h
mkc_string.o:  mkc_string.h mkc_nodiscard.h
mkc_util.o: mkc_util.h
mkc_var.o:   mkc_def.h
mkc_var.o:   mkc_error.h mkc_list.h
mkc_var.o: mkc_var.h mkc_log.h  mkc_string.h
mkc_var.o: mkc_nodiscard.h
