#!/bin/sh
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#
# This script rebuilds the object lists in bootstrap.mk
#
# The INITIALOBJ list is the files that
# include the mkc_config.h file.
#
# The NOREGEXOBJ list is the files that
# use any check that uses regular expressions, and any
# file that uses one of the variables in VERSION.txt (as
# these are extracted using regular expressions).
#
# Checks that use regular expressions:
#   _arg_count_
#   _shell_extract_
#
# The MKCOBJECTS list is ordered by dependency.
#

if [ $# -ne 1 ]; then
  echo "$0 : invalid arguments"
  exit 1
fi

BOOTSTRAPMAKE=$1
TMPFILE=tmp/bootstrapsetup-tmp.txt
TMPVARLIST=tmp/tmpvarlist.txt

LORDER=`which lorder`
if [ "$LORDER" = "" ]; then
  # linux doesn't ship lorder,
  # and doesn't even have it available for install
  LORDER=utils/lorder
fi

updbootstrapmake () {
  name=$1

  cat ${BOOTSTRAPMAKE} | \
      sed -e "/^${name}/,/^$/ d" | \
      sed -e "/^# ${name}/ r ${TMPFILE}" \
      > ${BOOTSTRAPMAKE}.n
  mv ${BOOTSTRAPMAKE}.n ${BOOTSTRAPMAKE}
  rm -f ${TMPFILE}
}

nm=INTIALOBJ
echo "${nm} = \\" > ${TMPFILE}
# mkc_grammar must be re-compiled, as it has a #if MKC_BOOTSTRAP
echo '\tmkc_grammar.o \\' >> ${TMPFILE}
# and re-compile anything that includes mkc_config.h
grep -E -l 'include "mkc_config.h' *.c | \
    sed -e 's,\.c$,\.o,' \
        -e 's,^,\t,' \
        -e '$ ! s,$, \\,' \
    >> ${TMPFILE}
echo '' >> ${TMPFILE}
updbootstrapmake ${nm}

nm=NOREGEXOBJ
echo "${nm} = \\" > ${TMPFILE}
# regular expressions are available
# anything that uses _arg_count_ must be re-compiled
# this will probably need to be expanded in the future.
grep -E -l 'if (_arg_count)' *.c | \
    sed -e 's,\.c$,\.o,' \
        -e 's,^,\t,' \
        -e '$ ! s,$, \\,' \
    >> ${TMPFILE}

# anything that uses one of the shell variables, in this case,
# any variable that is in VERSION.txt
grep '^[A-Z]=' VERSION.txt | \
    sed -e 's,=.*,,' \
    > ${TMPVARLIST}
grep -f ${TMPVARLIST} *.c |
    sed -e 's,\.c$,\.o,' \
        -e 's,^,\t,' \
        -e '$ ! s,$, \\,' \
    >> ${TMPFILE}
echo '' >> ${TMPFILE}
updbootstrapmake ${nm}

# if mkc is there, assume that the .o files are there.
if [ -f mkc ]; then
  nm=MKCOBJECTS
  echo "${nm} = \\" > ${TMPFILE}
  ${LORDER} *.o |
      tsort |
      sed -e '/mkc_os_process/ a \\t$(MKC_WIN_OBJ)\ \\' \
          -e '/mkc_regex_pcre/ s,mkc_regex_pcre\.o,$(MKC_REGEX_OBJ),' \
          -e 's,^,\t,' \
          -e '$ ! s,$, \\,' \
      >> ${TMPFILE}
  echo '' >> ${TMPFILE}
  updbootstrapmake ${nm}
fi

rm -f ${TMPFILE} ${TMPVARLIST}
exit 0
