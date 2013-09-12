set(TARGET libneon)
project(${TARGET})

cmake_minimum_required(VERSION 2.4)
if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set(LOCAL_PREFIX ${CMAKE_BINARY_DIR})

include(ExternalProject)
ExternalProject_Add(
   libneon
   SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
   PREFIX ${LOCAL_PREFIX}
   CONFIGURE_COMMAND ./build_neon.sh ${LOCAL_PREFIX}
   BUILD_IN_SOURCE 1
)




