#!/bin/sh
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

. ./tests/testutil.sh

tnm=$0
bnm=$(basename $0 | sed 's,\.sh$,,')
expfail=F

ttype=mkc

args="--no-cache"
dotest ${ddir}/${bnm}.mkc
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi
mv -f ${odir}/$bnm.out ${odir}/$bnm-a.out
dodiff ${rdir}/$bnm.out ${odir}/$bnm-a.out
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi
testfin

args=""
dotest ${ddir}/${bnm}.mkc
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi
mv -f ${odir}/$bnm.out ${odir}/$bnm-b.out
dodiff ${rdir}/$bnm.out ${odir}/$bnm-b.out
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi
testfin

dodiff ${odir}/$bnm-a.out ${odir}/$bnm-b.out
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi
