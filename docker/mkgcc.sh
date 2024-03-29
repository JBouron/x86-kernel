#!/bin/sh
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
# The $PREFIX/bin dir _must_ be in the PATH. We did that above.
which -- $TARGET-as || echo $TARGET-as is not in the PATH

NCPUS=$(lscpu | grep '^CPU(s):' | awk '{print $2}')
 
mkdir build-gcc
cd build-gcc
../gcc-9.1.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make all-gcc -j$NCPUS
make all-target-libgcc -j$NCPUS
make install-gcc -j$NCPUS
make install-target-libgcc -j$NCPUS
