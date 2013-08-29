#!/bin/sh

./autogen.sh
./configure --prefix=`pwd`/../build --enable-static --disable-shared --with-http --with-tree --without-iconv --without-icu --without-debug --with-html --without-python
