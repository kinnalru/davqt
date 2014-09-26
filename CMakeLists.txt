set(TARGET davqt)
project(${TARGET})

cmake_minimum_required(VERSION 2.4)
if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DDEBUG)
endif()

add_definitions("-std=c++0x -std=c++11")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Boost REQUIRED COMPONENTS
    program_options
    system
)
include_directories(${Boost_INCLUDE_DIRS})

find_package(Qt4 REQUIRED QtCore QtNetwork QtXml QtGui)
include(${QT_USE_FILE})

add_subdirectory(3rdparty/preferences)
add_subdirectory(3rdparty//qtwebdav)

set(HEADERS
    database/database.h
    database/db.h
    database/entry.h
    database/fs/fs.h
    storage.h

)

set(MOC
    handlers/handlers.h
    main_window.h
    manager.h
    session.h
    settings/main_settings.h
    settings/merge_settings.h
    settings/settings.h
    settings/sync_settings.h
    sync.h    
    syncer.h
    tools.h
    types.h
    updater.h
)

set(SOURCES 
    database/database.cpp
    database/entry.cpp
    database/fs/fs.cpp
    handlers/handlers.cpp
    main.cpp
    main_window.cpp
    manager.cpp
    session.cpp
    settings/main_settings.cpp
    settings/merge_settings.cpp
    settings/sync_settings.cpp
    storage.cpp
    sync.cpp
    syncer.cpp
    updater.cpp
)

set(IFACES
    main_window.ui
    settings/main_settings.ui
    settings/merge_settings.ui
    settings/sync_settings.ui
)

set(RESOURCES
    images.qrc
)

set(DEPS
	#    ${CMAKE_BINARY_DIR}/lib/libneon.a 
	#    ${CMAKE_BINARY_DIR}/lib/libxml2.a 
  qtwebdav
	neon
)

qt4_wrap_cpp(SOURCES ${MOC})
qt4_wrap_ui(SOURCES ${IFACES})
qt4_add_resources(SOURCES ${RESOURCES})

add_executable(${TARGET} WIN32 ${HEADERS} ${SOURCES})
target_link_libraries(${TARGET} ${Boost_LIBRARIES} ${QT_LIBRARIES} ${DEPS} preferences)



