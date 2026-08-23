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
mkclog=${MKCLOG}/mkc-log.txt
mkcintlog=${MKCLOG}/internal-log.txt

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

  ${prog} ${cachearg} ${args} ${tfile} > ${odir}/$bnm.out 2>>${LOG}
  trc=$?
  cachedisp=""
  if [ "${cachearg}" = "" ]; then
    cachedisp="(cached)"
  fi
  if [ $expfail = T ]; then
    if [ $trc -eq 0 ]; then
      echo "   fail: test: ${cachedisp} $tfile"
      trc=1
    else
      trc=0
    fi
  else
    if [ $trc -ne 0 ]; then
      echo "   fail: test: ${cachedisp} $tfile"
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
  if [ -f ${mkcintlog} ]; then
    echo "log-int: ${cachearg} ${tnm}" >> ${LOG}
    cat ${mkcintlog} | sed 's,^,  ,' >> ${LOG}
    rm -f ${mkcintlog}
  fi
  if [ -f ${mkclog} ]; then
    echo "log: ${cachearg} ${tnm}" >> ${LOG}
    cat ${mkclog} | sed 's,^,  ,' >> ${LOG}
    rm -f ${mkclog}
  fi
}
