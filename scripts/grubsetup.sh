#!/bin/sh
set -e
# check if `brew` is installed
command -v brew >/dev/null 2>&1 || { echo >&2 "It seems you do not have \`brew\` installed. Head on over to http://brew.sh/ to install it."; exit 1; }

export TARGET=i686-elf
export PREFIX="$HOME/bin"

mkdir -p $PREFIX

git clone git://git.savannah.gnu.org/grub.git
cd grub
./bootstrap
sh autogen.sh
cd ..
scripts/grub-build-fixes.sh

cd grub
mkdir -p build-grub
cd build-grub
../configure --disable-werror TARGET_CC=$TARGET-gcc TARGET_OBJCOPY=$TARGET-objcopy \
TARGET_STRIP=$TARGET-strip TARGET_NM=$TARGET-nm TARGET_RANLIB=$TARGET-ranlib --target=$TARGET --prefix=$PREFIX
make
make install