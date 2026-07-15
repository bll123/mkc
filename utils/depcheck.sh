#!/bin/sh
#
# Copyright 2021-2025 Brad Lanam Pleasant Hill CA
#

if [ \( ! -d web \) -o \( ! -d wiki \) ]; then
  echo "wrong dir"
  exit 1
fi
cwd=`pwd`

keep=F
if [ "$1" = --keep ]; then
  keep=T
fi

systype=`uname -s`

INCTC=dep-inctest.c
INCTO=dep-inctest.o
INCTOUT=dep-inctest.log
TIIN=dep-inc-in.txt
TISORT=dep-inc-sort.txt
TOIN=dep-obj-in.txt
TOSORT=dep-obj-sort.txt
grc=0

# check for missing copyrights
echo "## checking for missing copyright"

for fn in *.y *.l *.c *.mk include/*.h *.mkc \
    tests/tests/*.mkc tests/*.sh tests/tests/*.sh \
    tests/data/*.mkc tests/data/*.txt tests/data/*.h \
    utils/*.mkc templates/*.c; do
  case $fn in
    *000-empty.mkc)
      continue;
      ;;
    *tt.sh|*z.sh)
      continue
      ;;
    dev/*)
      continue
      ;;
    utils/*.sh)
      # most of this can be skipped
      continue
      ;;
    mkc_lex.c|mkc_lex.h|mkc_grammar.c|mkc_grammar.h)
      continue
      ;;
  esac
  grep "Copyright" $fn > /dev/null 2>&1
  rc=$?
  if [ $rc -ne 0 ]; then
    echo "$fn : missing copyright"
    grc=$rc
  fi
done
if [ $grc != 0 ]; then
  exit $grc
fi

# check the object file hierarchy for problems.
echo "## checking object file hierarchy"
#
tdir=.

OBJEXT=.o
LORD=./utils/lorder
case ${systype} in
  Darwin)
    LORD=lorder
    ;;
  MINGW*)
    # cygwin cmake uses .o
    # OBJEXT=.obj
    ;;
esac

${LORD} `find ${tdir} -name '*'${OBJEXT}` > $TOIN
tsort < $TOIN > $TOSORT
rc=$?
if [ $rc -ne 0 ]; then
  grc=$rc
fi

if [ $keep = F ]; then
  rm -f $TIIN $TISORT $TOIN $TOSORT > /dev/null 2>&1
fi
rm -f $INCCT $INCTO $INCTOUT cmake.log

exit $grc
