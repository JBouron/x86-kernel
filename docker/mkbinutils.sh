#!/bin/sh
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

NCPUS=$(lscpu | grep '^CPU(s):' | awk '{print $2}')

mkdir build-binutils
cd build-binutils
../binutils-2.32/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j $NCPUS
make install
