#!/bin/bash
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

. ./tests/testsetup.sh

tnm=$0
bnm=$(basename $0 | sed 's,\.sh$,,')
expfail=F

ttype=mkc

args="--no-cache -p debug"
dotest ${ddir}/073-profile-select.mkc
if [[ $rc -ne 0 ]]; then exit $rc; fi
mv -f ${odir}/$bnm.out ${odir}/$bnm-debug.out
dodiff ${rdir}/$bnm-debug.out ${odir}/$bnm-debug.out
testfin

dotest ${ddir}/073-profile-select.mkc
if [[ $rc -ne 0 ]]; then exit $rc; fi
mv -f ${odir}/$bnm.out ${odir}/$bnm-release.out
dodiff ${rdir}/$bnm-release.out ${odir}/$bnm-release.out
testfin
