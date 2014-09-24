set(TARGET davqt)
project(${TARGET})

cmake_minimum_required(VERSION 2.4)
if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DDEBUG)
endif()

add_definitions("-std=c++0x")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Boost REQUIRED COMPONENTS
    program_options
    system
)
include_directories(${Boost_INCLUDE_DIRS})

find_package(Qt4 REQUIRED QtCore QtNetwork QtGui)
include(${QT_USE_FILE})


#add_subdirectory(3rdparty/webdav/openssl)
#add_subdirectory(3rdparty/webdav/libxml2)
#add_subdirectory(3rdparty/webdav/neon)

add_subdirectory(3rdparty/preferences)

set(HEADERS
    database/database.h
    database/db.h
    database/entry.h
    database/fs/fs.h
    handlers.h
    storage.h

)

set(MOC
    main_window.h
    session.h
    settings/main_settings.h
    settings/settings.h
    sync.h    
    tools.h
    updater.h
    syncer.h
    types.h
    manager.h
)

set(SOURCES 
    database/database.cpp
    database/entry.cpp
    database/fs/fs.cpp
    handlers.cpp
    main.cpp
    main_window.cpp
    session.cpp
    settings/main_settings.cpp
    storage.cpp
    sync.cpp
    updater.cpp
    syncer.cpp
    manager.cpp
)

set(IFACES
    main_window.ui
    settings/main_settings.ui
)

set(RESOURCES
    images.qrc
)

set(DEPS
	#    ${CMAKE_BINARY_DIR}/lib/libneon.a 
	#    ${CMAKE_BINARY_DIR}/lib/libxml2.a 
	neon
)

qt4_wrap_cpp(SOURCES ${MOC})
qt4_wrap_ui(SOURCES ${IFACES})
qt4_add_resources(SOURCES ${RESOURCES})

add_executable(${TARGET} WIN32 ${HEADERS} ${SOURCES})
target_link_libraries(${TARGET} ${Boost_LIBRARIES} ${QT_LIBRARIES} ${DEPS} preferences)



