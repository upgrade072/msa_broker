#!/bin/bash

export ROOTDIR="$(pwd)/../"
export PKG_CONFIG_PATH=${ROOTDIR}/build/lib/pkgconfig
LIBNAME="libtcmalloc"

if [  -f ${ROOTDIR}/build/lib/${LIBNAME}.a ]; then
	echo "${LIBNAME}.a already exist in build/lib"
    exit
fi

VERSION="2.9.1"
if [ ! -f gperftools-${VERSION}.tar.gz ]; then
	wget https://github.com/gperftools/gperftools/archive/refs/tags/gperftools-${VERSION}.tar.gz
fi

rm -rf  gperftools-gperftools-${VERSION}
tar xvf  gperftools-${VERSION}.tar.gz

cd gperftools-gperftools-${VERSION}

autoreconf -i
./configure \
    --prefix=${ROOTDIR}/build \
	--with-tcmalloc-pagesize=16 \
    --enable-static \
    --disable-shared

make
make install
