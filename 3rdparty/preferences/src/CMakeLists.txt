set(TARGET preferences)

project(${TARGET})

#set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -std=c++0x)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt4 COMPONENTS QtCore QtGui REQUIRED)
include(${QT_USE_FILE})

#backing up compiler flags to prevent kde hidden magic
set ( CMAKE_CXX_FLAGS_BACK ${CMAKE_CXX_FLAGS} )
find_package(KDE4)
#restoring compiler flags
set ( CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS_BACK} )

if(KDE4_FOUND)
    add_definitions(${KDE4_ENABLE_EXCEPTIONS})
    add_definitions(-DUSE_KDE_DIALOG)
    set(KDE4_LIBS ${KDE4_KDEUI_LIBRARY})
endif()

set(PUBLIC_HEADERS
    preferences_item.h
    preferences_widget.h
    preferences_dialog.h
)

set(HEADERS
    ${PUBLIC_HEADERS}
    detail/connector.h
    detail/views.h
)

set(MOC_HEADERS
    preferences_widget.h
    preferences_dialog.h
    detail/connector.h
    detail/delegate.h
)

set(SOURCES
    preferences_item.cpp
    preferences_widget.cpp
    preferences_dialog.cpp
    detail/connector.cpp
)

set(IFACES
    ui/preferences_dialog.ui
)

qt4_wrap_cpp(SOURCES ${MOC_HEADERS})
qt4_wrap_ui(SOURCES ${IFACES})

add_library(${TARGET} STATIC ${SOURCES})

set_target_properties(${TARGET} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${TARGET} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

target_link_libraries(${TARGET} ${QT_LIBRARIES} ${KDE4_LIBS})

#install(TARGETS ${TARGET} RUNTIME DESTINATION bin)
#install(FILES GeoIP.dat DESTINATION share/urtconnector)
#install(FILES ${PUBLIC_HEADERS} DESTINATION ${CL_INSTALL_INC} COMPONENT Devel)


