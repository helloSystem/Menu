set(PLUGIN "statusnotifier")
project(${PLUGIN})

find_package(dbusmenu-qt5 REQUIRED)
find_package(KF5WindowSystem REQUIRED)

set(SRCS
    statusnotifier.h
    statusnotifier.cpp
    dbustypes.h
    dbustypes.cpp
    statusnotifierbutton.h
    statusnotifierbutton.cpp
    statusnotifieriteminterface.h
    statusnotifieriteminterface.cpp
    statusnotifierwatcher.h
    statusnotifierwatcher.cpp
    statusnotifierwidget.h
    statusnotifierwidget.cpp
    sniasync.h
    sniasync.cpp
    ../src/mainwindow.h
    ../src/mainwindow.cpp
)

#qt5_add_dbus_adaptor(DBUS_SOURCES
#    org.kde.StatusNotifierItem.xml
#    statusnotifieriteminterface.h
#    StatusNotifierItemInterface
#)

#set_source_files_properties(${DBUS_SOURCES} PROPERTIES SKIP_AUTOGEN ON)

add_definitions("${QT_DEFINITIONS} -DQT_PLUGIN")
add_library(${PLUGIN} SHARED ${SRCS})

target_link_libraries(${PLUGIN} PRIVATE
    Qt5::DBus
    Qt5::Widgets
    Qt5::Concurrent
    Qt5::X11Extras
    KF5::WindowSystem
    dbusmenu-qt5
)
target_include_directories(${PLUGIN} PUBLIC ../interfaces)

install(TARGETS ${PLUGIN} LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/Resources/plugins)
