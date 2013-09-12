#!/bin/sh


LOCAL_PREFIX=$1
PREFIX=$LOCAL_PREFIX

CPPFLAGS="-I${PREFIX}/include" LDFLAGS="-L${PREFIX}/lib" ./autogen.sh --prefix=${PREFIX} --enable-static --disable-shared --with-http --with-tree --without-iconv --without-icu --without-debug --with-html --without-python --without-zlib --without-lzma
