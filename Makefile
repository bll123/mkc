# clean

MAKEFLAGS += --no-print-directory
BOOTSTRAPMAKE = bootstrap.mk

# if the bootstrap has been run, then the Makefile will be 
# populated with the mkc generated output, and the mkc-all
# target will exist
.PHONY: all
all:
	@grep '^mkc-all' Makefile >/dev/null 2>&1; rc=$$?; \
	if [ $$rc -eq 0 ]; then \
	  $(MAKE) mkc-all; \
	else \
	  echo "-- bootstrap mkc" ; \
	  $(MAKE) bootstrap ; \
	fi

.PHONY:
bootstrap:
	@$(MAKE) -f $(BOOTSTRAPMAKE) all

# remove the generated files
.PHONY: distclean
distclean:
	@$(MAKE) realclean
	@-rm -f mkc_grammar.h mkc_grammar.c
	@-rm -f mkc_lex.h mkc_lex.c

# keep the generated files
# these will be shipped with a distribution
.PHONY: realclean
realclean:
	@$(MAKE) clean

.PHONY: clean
clean:
	@$(MAKE) tclean
	@-rm -f *.o
	@-rm -f mkc
	@-#rm -rf mkc_files

.PHONY: tclean
tclean:
	@-find . -name '*.orig' -print | xargs rm -f
	@-find . -name '*~' -print | xargs rm -f
	@-rm -f w ww www *.orig asan.* core.*
	@-rm -f mkc_grammar.output *.mk.bak
	@-rm -f tests/tmp/*
	@-rm -f mkc_files/tmp/*

# depend for bootstrap makefile

.PHONY:depend
depend:
	-makedepend -f $(BOOTSTRAPMAKE) -Iinclude *.c 2>/dev/null
	cat $(BOOTSTRAPMAKE) | \
	    sed -e 's,[/]usr[/]include[/][^ ]*,,g' \
	    -e '/^[a-z][a-z_]*\.o:[ ]*$$/ d' \
	    > $(BOOTSTRAPMAKE).n
	mv $(BOOTSTRAPMAKE).n $(BOOTSTRAPMAKE)

# MKC DO NOT DELETE
