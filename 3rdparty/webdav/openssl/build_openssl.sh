#!/bin/sh

LOCAL_PREFIX=$1
PREFIX=$LOCAL_PREFIX

CPPFLAGS="-I${PREFIX}/include" LDFLAGS="-L${PREFIX}/lib" ./config --prefix=${PREFIX} no-shared
