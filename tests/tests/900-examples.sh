#!/bin/sh
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

# try all examples

. ./tests/testutil.sh

tnm=$0
bnm=`basename $0 | sed 's,\.sh$,,'`
expfail=F

ttype=none

cwd=`pwd`

chk900() {
  cachearg=$1

  cachedisp=""
  if [ "${cachearg}" = "" ]; then
    cachedisp="(cached)"
  fi

  echo "$tnm: $enm: ${cachedisp}"
  # all examples have hw.mkc (as of 2026-9-27)
  ../../mkc ${cachearg} --clean --prefix '' hw.mkc
  rc=$?
  if [ $rc -ne 0 ]; then
    echo "   fail: test: ${enm} ${cachedisp} $tnm"
    exit $rc
  fi
  if [ ! -x mkc_files/hw/stage/bin/hw ]; then
    echo "   fail: test: ${enm} (no executable) ${cachedisp} $tnm"
    exit 1
  fi
  out=`./mkc_files/hw/stage/bin/hw`
  rc=$?
  if [ $rc -ne 0 ]; then
    echo "   fail: test: ${enm} (run failed) ${cachedisp} $tnm"
    exit $rc
  fi
  if [ "$out" != "hello world" ]; then
    echo "   fail: test: ${enm} (incorrect output) ${cachedisp} $tnm"
    exit 1
  fi
}

for e in examples/*; do
  cd ${e}

  enm=`basename ${e}`

  rc=$?
  if [ $rc -ne 0 ]; then
    echo "   fail: test: ${enm} (chdir) $tnm"
    cd ${cwd}
    exit $rc
  fi

  chk900 '--no-cache'
  chk900
  cd ${cwd}
done

testfin
