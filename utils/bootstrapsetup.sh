#!/bin/sh
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#
# This script rebuilds the object lists in bootstrap.mk
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

MAKEFILE=$1
TMPDIR=mkc-tmp
TMPFILE=${TMPDIR}/bootstrapsetup-tmp.txt
TMPVARLIST=${TMPDIR}/tmpvarlist.txt
TMPFLIST=${TMPDIR}/tmpfilelist.txt
OBJDIR=mkc_files/mkc/obj

LORDER=`which lorder`
if [ "$LORDER" = "" ]; then
  # linux doesn't ship lorder,
  # and doesn't even have it available for install
  LORDER=utils/lorder
fi
TAC=`which tac`
if [ "$TAC" = "" ]; then
  TAC=cat
fi

updbootstrapmake () {
  name=$1

  sed -e "/^${name} = /,/^$/ d" ${MAKEFILE} | \
  sed -e "/^# ${name} keep/ r ${TMPFILE}" \
      > ${MAKEFILE}.n
  mv ${MAKEFILE}.n ${MAKEFILE}
  rm -f ${TMPFILE}
}

# if mkc is there, assume that the .o files are there.
if [ -f mkc ]; then
  nm=MKCOBJECTS
  echo "${nm} = \\" > ${TMPFILE}
  ${LORDER} ${OBJDIR}/*.o |
      grep -v 'topochk' |
      tsort |
      ${TAC} |
      sed -e '/os_process/ a \\t$(MKC_WIN_OBJ)\ \\' \
          -e '/os_win_process/ d' \
          -e '/mkc_regex_pcre/ s,mkc_regex_pcre\.o,$(MKC_REGEX_OBJ),' \
          -e 's,^,\t,' \
          -e '$ ! s,$, \\,' \
          -e "s,${OBJDIR}/,," \
      >> ${TMPFILE}
  echo '' >> ${TMPFILE}
  updbootstrapmake ${nm}
fi

rm -f ${TMPFILE} ${TMPVARLIST} ${TMPFLIST}
exit 0
