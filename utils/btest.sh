#!/bin/sh
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

. ./VERSION.txt

cwd=`pwd`
LOG=${cwd}/btest.log

archtag=""

systype=`uname -s`
arch=`uname -m`
case ${systype} in
  Linux)
    tag=linux
    ;;
  Darwin)
    tag=macos
    ;;
  MINGW*)
    tag=windows
    ;;
  *BSD)
    tag=freebsd
    ;;
esac

rtag=""
if [ $RELEASELEVEL != production ]; then
  rtag="-${RELEASELEVEL}"
fi

> $LOG

echo "-- build ${tag}"
echo "-- build ${tag}" >> $LOG
echo "-- path: ${PATH}" >> $LOG
make distclean >> $LOG 2>&1

(make;rc=$?;exit $rc) >> $LOG 2>&1
rc=$?
if [ $rc -ne 0 ]; then
  echo "== build failed"
  exit $rc
fi

echo "-- test ${tag}"
echo "-- test ${tag}" >> $LOG
(./tests/runtests.sh;rc=$?;exit $rc) >> $LOG 2>&1
rc=$?
if [ $rc -ne 0 ]; then
  echo "== tests failed"
  exit $rc
fi

exit 0
