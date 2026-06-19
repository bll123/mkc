#!/bin/bash

tdir=tests/tests
ddir=tests/data
rdir=tests/results
odir=tests/tmp
LANG=C
MKCTMP=tests/tmp
LOG=${MKCTMP}/log-runtests.txt
MKCLOG=mkc_files
mkclog=${MKCLOG}/log-mkc.txt

test -d ${odir} || mkdir -p ${odir}
test -d ${MKCTMP} || mkdir -p ${MKCTMP}
test -f ${LOG} && rm -f ${LOG}

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
      prog=./mkc
      args=--no-cache
      ;;
    *.sh)
      prog=bash
      args=""
      ;;
  esac

  echo "== $tnm"
  echo "== $tnm" >> ${LOG}
  bnm=$(basename $tnm | sed 's,\.mkc$,,')
  expfail=F
  case $tnm in
    *-error.*)
      expfail=T
      ;;
  esac

  ${prog} ${args} $tnm > ${odir}/$bnm.out 2>>${LOG}
  rc=$?
  if [[ $expfail == T ]]; then
    if [[ $rc -eq 0 ]]; then
      echo "   fail: test: $tnm"
      continue
    fi
  else
    if [[ $rc -ne 0 ]]; then
      echo "   fail: test: $tnm"
      continue
    fi
  fi

  diff=F
  if [[ -f ${rdir}/$bnm.h ]]; then
    diff=T
    diff -q -w ${rdir}/$bnm.h ${odir}/$bnm.h >>${LOG} 2>&1
    rc=$?
  elif [[ -f ${rdir}/$bnm.out ]]; then
    diff=T
    diff -q -w ${rdir}/$bnm.out ${odir}/$bnm.out >>${LOG} 2>&1
    rc=$?
  fi

  if [[ $diff == T ]]; then
    if [[ $rc -ne 0 ]]; then
      echo "   fail: diff: $tnm"
    else
      true
      # echo "   ok: diff: $tnm"
    fi
  fi

  if [[ -f ${mkclog} ]]; then
    mv ${mkclog} ${MKCTMP}/log-${bnm}.txt
  fi
done
