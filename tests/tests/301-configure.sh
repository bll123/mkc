#!/bin/sh
#
# Copyright 2026 Brad Lanam Pleasant Hill CA
#

# tests to make sure the auto-generated name is correct.
# tests with a single .mkc and with an included .mkc

. ./tests/testutil.sh

tnm=$0
bnm=$(basename $0 | sed 's,\.sh$,,')
expfail=F

ttype=mkc

chk301 () {
  tag=$1

  # old pre-alpha bug
  if [ -f mkctest_config_config.h ]; then
    echo "  ${tag}: invalid name: mkctest_config_config.h"
    rm -f mkctest_config_config.h
    return 1
  fi
  if [ -f project_config_config.h ]; then
    echo "  ${tag}: invalid name: project_config_config.h"
    rm -f project_config_config.h
    return 1
  fi

  # if the project name is not picked up
  if [ -f project_config.h ]; then
    echo "  ${tag}: invalid name: project_config.h"
    rm -f project_config.h
    exit 1
  fi

  # check for the proper name
  if [ -f mkctest_config.h ]; then
    mv -f mkctest_config.h ${odir}/$bnm.h
  else
    echo "  ${tag}: not found: mkctest_config.h"
    return 1
  fi

  return 0
}

rm -f mkctest_config.h project_config.h 2>/dev/null

# basic test to make sure mkctest_config.h is created
dotest ${ddir}/301-configure.mkc
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi
chk301 basic
dodiff ${rdir}/$bnm.h ${odir}/$bnm.h
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi

# include test.  the project name should not be 'project'.
dotest ${ddir}/${bnm}-a.mkc
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi
chk301 include
dodiff ${rdir}/$bnm.h ${odir}/$bnm.h
rc=$?
if [ $rc -ne 0 ]; then exit $rc; fi

testfin
