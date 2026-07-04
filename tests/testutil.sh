#!/bin/sh
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

tdir=tests/tests
ddir=tests/data
rdir=tests/results
odir=tests/tmp
LANG=C
MKCTMP=tests/tmp
LOG=${MKCTMP}/log-runtests.txt
MKCLOG=mkc_files
mkclog=${MKCLOG}/log-mkc.txt

MKC=${MKC:-./mkc}

dotest () {
  tfile=$1

  if [ $ttype = mkc ]; then
    prog=${MKC}
  fi
  if [ $ttype = sh ]; then
    prog="sh "
    args=""
  fi

  ${prog} ${args} ${tfile} > ${odir}/$bnm.out 2>>${LOG}
  trc=$?
  if [ $expfail = T ]; then
    if [ $trc -eq 0 ]; then
      echo "   fail: test: $tfile"
    fi
  else
    if [ $trc -ne 0 ]; then
      echo "   fail: test: $tfile"
    fi
  fi

  return $trc
}

dodiff () {
  dfile=$1
  ofile=$2
  trc=0

  diff=F
  if [ "$dfile" = "" ]; then
    if [ -f ${rdir}/$bnm.h ]; then
      diff=T
      dfile=${rdir}/$bnm.h
      ofile=${odir}/$bnm.h
    fi
    if [ -f ${rdir}/$bnm.out ]; then
      diff=T
      dfile=${rdir}/$bnm.out
      ofile=${odir}/$bnm.out
    fi
  fi

  if [ ${diff} = T ]; then
    diff=T
    diff -q -w ${dfile} ${ofile} >>${LOG} 2>&1
    trc=$?

    if [ $trc -ne 0 ]; then
      echo "   fail: diff: $tnm"
    else
      true
      # echo "   ok: diff: $tnm"
    fi
  fi

  return $trc
}

testfin () {
  if [ -f ${mkclog} ]; then
    mv ${mkclog} ${MKCTMP}/${bnm}-log.txt
  fi
}
