#!/bin/sh


LOCAL_PREFIX=$1
PREFIX=$LOCAL_PREFIX

CPPFLAGS="-I${PREFIX}/include" LDFLAGS="-L${PREFIX}/lib" ./configure --prefix=${PREFIX} --enable-static --disable-shared --with-ssl=openssl --enable-webdav --disable-debug --with-libxml2 --without-libproxy 
