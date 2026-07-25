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

test -f ${LOG} && rm -f ${LOG}
test -d ${MKCTMP} && rm -rf ${MKCTMP}
test -d ${MKCTMP} || mkdir -p ${MKCTMP}
test -d ${odir} || mkdir -p ${odir}

target=""
STOPONFAIL=F
while test $# -gt 0; do
  case $1 in
    --stoponfail)
      STOPONFAIL=T
      shift
      ;;
    *)
      target=$1
      shift
      ;;
  esac
done

pattern="*.[sm]*"
case $target in
  [0-9][0-9]*)
    val=$target
    pattern="${val}*.[sm]*"
    ;;
esac

for tnm in ${tdir}/${pattern}; do
  case ${tnm} in
    *~)
      continue
      ;;
    *.mkc)
      ttype=mkc
      args="--no-cache --profile default"
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
  if [ \( $STOPONFAIL = T \) -a \( $rc -ne 0 \) ]; then
    exit $rc
  fi
  if [ $rc -ne 0 ]; then continue; fi
  if [ $ottype = mkc ]; then
    # shell scripts run their own diff...
    dodiff
    rc=$?
    if [ \( $STOPONFAIL = T \) -a \( $rc -ne 0 \) ]; then
      exit $rc
    fi
  fi
  testfin

  if [ $ottype = mkc ]; then
    if [ -f ${ddir}/${bnm}.cache ]; then
      args=""
      echo "== $tnm (cache)"
      echo "== $tnm (cache)" >> ${LOG}
      dotest ${tnm}
      rc=$?
      if [ \( $STOPONFAIL = T \) -a \( $rc -ne 0 \) ]; then
        exit $rc
      fi
      if [ $rc -ne 0 ]; then continue; fi
      dodiff
      rc=$?
      if [ \( $STOPONFAIL = T \) -a \( $rc -ne 0 \) ]; then
        exit $rc
      fi
      testfin
    fi
  fi
done

