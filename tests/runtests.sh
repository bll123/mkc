#!/bin/sh
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

. ./tests/testutil.sh

echo "-- using ${MKC}"

systype=`uname -s`
case ${systype} in
  Linux)
    tag=linux
    ;;
  Darwin)
    tag=macos
    ;;
  MINGW64*|CYGWIN*)
    tag=win64
    ;;
  MINGW32*)
    echo "Platform not supported"
    exit 1
    ;;
esac

if [ $tag = win64 ]; then
  MKCDFLTPROF="$USERPROFILE/AppData/Roaming/mkc/defaultprofile.txt"
else
  MKCDFLTPROF="$HOME/.config/mkc/defaultprofile.txt"
fi

if [ -f ${MKCDFLTPROF} ]; then
  mv ${MKCDFLTPROF} ${MKCDFLTPROF}.keep
fi

test -f ${LOG} && rm -f ${LOG}
test -d ${MKCTMP} && rm -rf ${MKCTMP}
test -d ${MKCTMP} || mkdir -p ${MKCTMP}
test -d ${odir} || mkdir -p ${odir}

pattern="*.[sm]*"
while test $# -gt 0; do
  case $1 in
    [0-9][0-9]*)
      val=$1
      pattern="${val}*.[sm]*"
      shift
      ;;
  esac
done

for tnm in ${tdir}/${pattern}; do
  case ${tnm} in
    *~)
      continue
      ;;
    *.mkc)
      ttype=mkc
      args=--no-cache
      ;;
    *.sh)
      ttype=sh
      args=""
      ;;
  esac

  echo "== $tnm"
  echo "== $tnm" >> ${LOG}
  bnm=`basename $tnm | sed 's,\.mkc$,,'`
  expfail=F
  case $tnm in
    *-error.*)
      expfail=T
      ;;
  esac

  ottype=${ttype}
  dotest ${tnm}
  rc=$?
  if [ $rc -ne 0 ]; then continue; fi
  if [ $ottype = mkc ]; then
    # shell scripts run their own diff...
    dodiff
  fi
  testfin

  if [ $ottype = mkc ]; then
    if [ -f ${ddir}/${bnm}.cache ]; then
      args=""
      echo "== $tnm (cache)"
      echo "== $tnm (cache)" >> ${LOG}
      dotest ${tnm}
      if [ $rc -ne 0 ]; then continue; fi
      dodiff
      testfin
    fi
  fi
done

if [ -f ${MKCDFLTPROF}.keep ]; then
  mv ${MKCDFLTPROF}.keep ${MKCDFLTPROF}
fi
