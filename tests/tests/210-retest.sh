#!/bin/sh
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

. ./tests/testutil.sh

tnm=$0
bnm=$(basename $0 | sed 's,\.sh$,,')
expfail=F

ttype=mkc

cp -f ${ddir}/210-retest-a.h ${odir}
rm -f ${odir}/210-retest-b.h
dotest ${ddir}/210-retest.mkc
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi
dodiff ${rdir}/$bnm-a.out ${odir}/$bnm-a.out
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi
testfin

cp -f ${ddir}/210-retest-b.h ${odir}
args=""
dotest ${ddir}/210-retest.mkc
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi
dodiff ${rdir}/$bnm-b.out ${odir}/$bnm-b.out
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi
testfin
