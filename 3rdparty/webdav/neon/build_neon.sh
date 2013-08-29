#!/bin/sh

PREFIX=`pwd`/../build
CPPFLAGS="-I${PREFIX}/include" LDFLAGS="-L${PREFIX}/lib" ./configure --prefix=${PREFIX} --enable-static --disable-shared --with-ssl=openssl --enable-webdav --disable-debug --with-libxml2 && make && make install
