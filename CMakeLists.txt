set(TARGET davqt)
project(${TARGET})

cmake_minimum_required(VERSION 2.4)
if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)



add_definitions("-std=c++0x")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Boost REQUIRED COMPONENTS
    program_options
	system
)
include_directories(${Boost_INCLUDE_DIRS})

find_package(Qt4 REQUIRED QtCore QtNetwork QtGui)
include(${QT_USE_FILE})


include_directories(${NEON_PREFIX}/include)
set(CMAKE_PREFIX_PATH "${CMAKE_SYSTEM_PREFIX_PATH} ${NEON_PREFIX}/lib")

set(NEON_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/neon)
include(ExternalProject)
ExternalProject_Add(
   localneon
   SOURCE_DIR ${NEON_PREFIX}
   CONFIGURE_COMMAND ${NEON_PREFIX}/configure --prefix=${NEON_PREFIX} --enable-webdav
   --enable-static --disable-shared --with-ssl=openssl --without-libproxy --without-ppsaaa
   --enable-debug
   PREFIX ${NEON_PREFIX}
   BUILD_COMMAND make
   BUILD_IN_SOURCE 1
)

add_subdirectory(3rdparty/preferences)

set(HEADERS
    database.h
    handlers.h
)

set(MOC
    main_window.h
    session.h
    sync.h    
    tools.h
    settings/settings.h
    settings/main_settings.h
)

set(SOURCES 
    database.cpp
    handlers.cpp
    main.cpp
    main_window.cpp
    session.cpp
    sync.cpp
    settings/main_settings.cpp
)

set(IFACES
    main_window.ui
    settings/main_settings.ui
)

set(RESOURCES
    images.qrc
)

set(DEPS
)

qt4_wrap_cpp(SOURCES ${MOC})
qt4_wrap_ui(SOURCES ${IFACES})
qt4_add_resources(SOURCES ${RESOURCES})

add_executable(${TARGET} WIN32 ${HEADERS} ${SOURCES})
add_dependencies(${TARGET} localneon)
target_link_libraries(${TARGET} ${Boost_LIBRARIES} ${QT_LIBRARIES} ${DEPS} ${NEON_PREFIX}/lib/libneon.a preferences)



