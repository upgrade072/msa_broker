#!/bin/bash

export ROOTDIR="$(pwd)/../"
export PKG_CONFIG_PATH=${ROOTDIR}/build/lib/pkgconfig
LIBNAME="libzmq"

if [  -f ${ROOTDIR}/build/lib/${LIBNAME}.a ]; then
	echo "${LIBNAME}.a already exist in build/lib"
    exit
fi

VERSION="4.3.4"
if [ ! -f zeromq-${VERSION}.tar.gz ]; then
	wget https://github.com/zeromq/libzmq/releases/download/v${VERSION}/zeromq-${VERSION}.tar.gz
fi

rm -rf zeromq-${VERSION}
tar xvf zeromq-${VERSION}.tar.gz

cp -f ./zhelpers.h ${ROOTDIR}/build/include
cd zeromq-${VERSION}

./configure \
    --prefix=${ROOTDIR}/build \
    --enable-static \
    --disable-shared

make
make install
