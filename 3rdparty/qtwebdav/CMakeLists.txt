set(TARGET qtwebdav)

project(${TARGET})

find_package(Qt4 COMPONENTS QtCore QtNetwork QtXml REQUIRED)
include(${QT_USE_FILE})



set(HEADERS
    webdav_url_info.h
)

set(MOC
    webdav.h
)

set(SOURCES
    webdav.cpp
    webdav_url_info.cpp
)


qt4_wrap_cpp(SOURCES ${MOC})
qt4_wrap_ui(SOURCES ${IFACES})

add_library(${TARGET} SHARED ${SOURCES})
target_link_libraries(${TARGET} ${QT_LIBRARIES})




