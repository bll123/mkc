#!/bin/bash

tdir=tests/tests
rdir=tests/results
odir=tests/tmp
LANG=C
MKCTMP=mkc_files/tmp
LOG=${MKCTMP}/log-runtests.txt
MKCLOG=mkc_files
mkclog=${MKCLOG}/log-mkc.txt

test -d ${odir} || mkdir -p ${odir}
test -d ${MKCTMP} || mkdir -p ${MKCTMP}
test -f ${LOG} && rm -f ${LOG}

for tnm in ${tdir}/*.mkc; do
  echo "== $tnm"
  echo "== $tnm" >> ${LOG}
  bnm=$(basename $tnm | sed 's,\.mkc$,,')
  expfail=F
  case $tnm in
    *error.mkc)
      expfail=T
      ;;
  esac

  if [[ $expfail == T ]]; then
    ./mkc $tnm > ${odir}/$bnm.out 2>>${LOG}
    rc=$?
    if [[ $rc -eq 0 ]]; then
      echo "   fail: test: $tnm"
      continue
    fi
  else
    ./mkc $tnm > ${odir}/$bnm.out 2>>${LOG}
    rc=$?
    if [[ $rc -ne 0 ]]; then
      echo "   fail: test: $tnm"
      continue
    fi
  fi

  if [[ -f ${rdir}/$bnm.out ]]; then
    diff -q -w ${rdir}/$bnm.out ${odir}/$bnm.out >>${LOG} 2>&1
    rc=$?
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
