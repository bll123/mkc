#!/bin/bash
#
# Copyright 2024-2025 Brad Lanam Pleasant Hill CA
#

SFUSER=bll123
sfproj=mkconf
project=mkc

. ./VERSION.txt
rlstag=""
if [[ $RELEASELEVEL != "" ]]; then
  rlstag="-${RELEASELEVEL}"
fi

if [[ ! -f ${project}-src-${VERSION}${rlstag}.tar.gz ]]; then
  echo "no source tar"
  exit 1
fi

make tclean

fn=README.md
rsync -v -e ssh ${fn} \
    ${SFUSER}@frs.sourceforge.net:/home/frs/project/${sfproj}/${fn}

rsync -v -e ssh ${project}-src-${VERSION}${rlstag}.tar.gz \
    ${SFUSER}@frs.sourceforge.net:/home/frs/project/${sfproj}/

exit 0
