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


add_subdirectory(3rdparty/webdav/openssl)
add_subdirectory(3rdparty/webdav/libxml2)
add_subdirectory(3rdparty/webdav/neon)

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
    ${CMAKE_BINARY_DIR}/lib/libneon.a 
    ${CMAKE_BINARY_DIR}/lib/libxml2.a 
)

qt4_wrap_cpp(SOURCES ${MOC})
qt4_wrap_ui(SOURCES ${IFACES})
qt4_add_resources(SOURCES ${RESOURCES})

add_executable(${TARGET} WIN32 ${HEADERS} ${SOURCES})
add_dependencies(${TARGET} libxml2 libopenssl libneon)
target_link_libraries(${TARGET} ${Boost_LIBRARIES} ${QT_LIBRARIES} ${DEPS} preferences)



