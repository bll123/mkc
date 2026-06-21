#!/bin/bash
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

. ./tests/testsetup.sh

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
  bnm=$(basename $tnm | sed 's,\.mkc$,,')
  expfail=F
  case $tnm in
    *-error.*)
      expfail=T
      ;;
  esac

  ottype=${ttype}
  dotest ${tnm}
  if [[ $rc -ne 0 ]]; then continue; fi
  if [[ $ottype == mkc ]]; then
    # shell scripts run their own diff...
    dodiff
  fi
  testfin

  if [[ $ottype == mkc ]]; then
    if [[ -f ${ddir}/${bnm}.cache ]]; then
      args=""
      echo "== $tnm (cache)"
      echo "== $tnm (cache)" >> ${LOG}
      dotest ${tnm}
      if [[ $rc -ne 0 ]]; then continue; fi
      dodiff
      testfin
    fi
  fi
done
