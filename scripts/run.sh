#!/bin/bash
# This script is used to build and run the ZineOS project.
# Run this after running build.sh
# Usage: ./run.sh [--gui]

MAKE_ARGS=""

if [ "$1" == "--gui" ]; then
    MAKE_ARGS="GUI=1"
fi

make $MAKE_ARGS
./scripts/install.sh
make run $MAKE_ARGS