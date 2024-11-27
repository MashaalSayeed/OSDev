#!/bin/bash

ARCH=$1

if [ -z "$ARCH" ]; then
  ARCH=i686
fi

scripts/grubsetup.sh

make ARCH=$ARCH