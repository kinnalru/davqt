#!/bin/sh


PREFIX=`pwd`/../build
CPPFLAGS="-I${PREFIX}/include" LDFLAGS="-L${PREFIX}/lib" ./autogen.sh --prefix=${PREFIX} --enable-static --disable-shared --with-http --with-tree --without-iconv --without-icu --without-debug --with-html --without-python
CPPFLAGS="-I${PREFIX}/include" LDFLAGS="-L${PREFIX}/lib" ./configure --prefix=${PREFIX} --enable-static --disable-shared --with-http --with-tree --without-iconv --without-icu --without-debug --with-html --without-python && make && make install
