#!/bin/sh
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

. ./VERSION.txt

cwd=`pwd`
LOG=${cwd}/pkg.log

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

sfx="${VERSION}${rtag}"

echo "-- create source package ${sfx}"
SRCDIR=mkc-${sfx}
SRCDEST=${SRCDIR}

rm -rf ${SRCDEST}
mkdir -p ${SRCDEST}

fn=VERSION.txt
cat > ${fn}.n << _HERE_
VERSION=${VERSION}
BUILD=`expr $BUILD + 1`
BUILDDATE=`date '+%Y-%m-%d'`
RELEASELEVEL=${RELEASELEVEL}
DEVELOPMENT=
_HERE_
mv ${fn}.n ${fn}

cp -pr include templates tests utils ${SRCDEST}
cp -p *.c *.y *.l *.h *.md *.mk *.mkc *.txt Makefile ${SRCDEST}

rm -f ${SRCDEST}/mkc_config.h

TARNM=mkc-src-${sfx}
tar -c -z -f ${TARNM}.tar.gz ${SRCDIR} >> $LOG 2>&1

rm -rf ${SRCDEST}

exit 0
