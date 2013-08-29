#!/bin/sh

mkdir build
(cd libxml2 && ./build_libxml2.sh) && (cd openssl && ./build_openssl.sh) && (cd neon && ./build_neon.sh)
