#!/bin/sh

PREFIX=`pwd`/../build
CPPFLAGS="-I${PREFIX}/include" LDFLAGS="-L${PREFIX}/lib" ./config --prefix=${PREFIX} no-shared && make && make install
