# Copyright 2026 Brad Lanam Pleasant Hill CA
#
# this makefile is only used for development and alpha releases

MAKEFILE = install.mk
PREFIX = 
INST_BIN = $(PREFIX)/bin
INST_SHARE = $(PREFIX)/share/mkc
INST_SHARE_TMPL = $(INST_SHARE)/templates
INST_SHARE_INC = $(INST_SHARE)/include
INST_SHARE_EXAMPLES = $(INST_SHARE)/examples

CP = cp

.PHONY: install
install:
	@if [ "$(PREFIX)" = "" ]; then echo "No prefix set"; exit 1; fi
	$(MAKE) -f $(MAKEFILE) TARGET=$@ oscheck

.PHONY: oscheck
oscheck:
	@if [ `./utils/chkforwin.sh` = T ]; then \
	  $(MAKE) -f $(MAKEFILE) windows-$(TARGET) ; \
	else \
	  $(MAKE) -f $(MAKEFILE) other-$(TARGET) ; \
	fi

.PHONY: windows-install
windows-install:
	$(MAKE) -f $(MAKEFILE) EXEEXT=".exe" real-$(TARGET)

.PHONY: other-install
other-install:
	$(MAKE) -f $(MAKEFILE) EXEEXT="" real-$(TARGET)

.PHONY: real-install
real-install:
	test -d $(INST_BIN) || mkdir -p $(INST_BIN)
	test -d $(INST_SHARE_INC) || mkdir -p $(INST_SHARE_INC)
	test -d $(INST_SHARE_TMPL) || mkdir -p $(INST_SHARE_TMPL)
	test -d $(INST_SHARE_EXAMPLES) || mkdir -p $(INST_SHARE_EXAMPLES)
	$(CP) mkc$(EXEEXT) $(INST_BIN)
	$(CP) include/mkc_compiler.h $(INST_SHARE_INC)
	$(CP) include/mkc_def.h $(INST_SHARE_INC)
	$(CP) -r templates $(INST_SHARE)
