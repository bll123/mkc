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
start=""
START=F
STOPONFAIL=F
while test $# -gt 0; do
  case $1 in
    --start)
      START=T
      shift
      start=$1
      shift
      ;;
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
  cache=F
  cachearg=""
  case ${tnm} in
    *~)
      continue
      ;;
    *.mkc)
      ttype=mkc
      args="--profile default"
      cache=T
      ;;
    *.sh)
      ttype=sh
      args=""
      ;;
  esac

  if [ $START = T ]; then
    case ${tnm} in
      */${start}-*)
        START=F
        ;;
      *)
        continue
        ;;
    esac
  fi

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
  if [ $cache = T ]; then
    cachearg="--no-cache"
  fi
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

  if [ -f ${ddir}/${bnm}.nocache ]; then
    cache=F
  fi
  if [ \( $cache = T \) -a \( $expfail = F \) ]; then
    cachearg=""
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
  fi
done

