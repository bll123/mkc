#!/bin/sh
#
# Copyright 2026 Brad Lanam Pleasant Hill CA

export twin=F
tmp=$(uname -s)
case ${tmp} in
  MINGW*) twin=T ;;
  CYGWIN*) twin=T ;;
esac
echo ${twin}
