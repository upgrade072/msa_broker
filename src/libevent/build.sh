#!/bin/bash

export ROOTDIR="$(pwd)/../"
export PKG_CONFIG_PATH=${ROOTDIR}/build/lib/pkgconfig
LIBNAME="libevent"

if [  -f ${ROOTDIR}/build/lib/${LIBNAME}.a ]; then
	echo "${LIBNAME} already exist in build/lib"
    exit
fi

VERSION="2.1.8-stable"
if [ ! -f libevent-${VERSION}.tar.gz ]; then
    wget https://github.com/libevent/libevent/releases/download/release-${VERSION}/libevent-${VERSION}.tar.gz
fi

rm -rf libevent-${VERSION}
tar xvf libevent-${VERSION}.tar.gz

cd libevent-${VERSION}

./configure \
    --prefix=${ROOTDIR}/build \
    --enable-static \
    --disable-shared \
    CFLAGS="$(CFLAGS) -I${ROOTDIR}/build/include" \
    LDFLAGS="$(LDFLAGS) -L${ROOTDIR}/build/lib" \
    LIBS="$(LIBS) -L${ROOTDIR}/build/lib -ldl -lpthread"

make
make install
