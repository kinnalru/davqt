project(preferences)
cmake_minimum_required(VERSION 2.4)
#set(CMAKE_VERBOSE_MAKEFILE ON)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

add_definitions("-std=c++0x")

add_subdirectory(src)
add_subdirectory(tests)
