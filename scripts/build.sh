#!/bin/bash

ARCH=$1

if [ -z "$ARCH" ]; then
  ARCH=i386
fi

make ARCH=$ARCH