set(TARGET panda-statusbar)

set(SRCS
    main.cpp
    mainwindow.h
    mainwindow.cpp
    mainpanel.h
    mainpanel.cpp
    extensionwidget.h
    extensionwidget.cpp
    blurwindow.h
    blurwindow.cpp
    popupwindow.h
    popupwindow.cpp
    pluginmanager.h
    pluginmanager.cpp
    appmenuwidget.cpp
    appmenuwidget.h
    appmenu/appmenumodel.h
    appmenu/appmenumodel.cpp
    appmenu/dbusmenuimporter.h
    appmenu/dbusmenuimporter.cpp
    appmenu/dbusmenu_interface.h
    appmenu/dbusmenu_interface.cpp
    appmenu/dbusmenushortcut_p.h
    appmenu/dbusmenushortcut_p.cpp
    appmenu/dbusmenutypes_p.h
    appmenu/dbusmenutypes_p.cpp
    appmenu/menuimporteradaptor.h
    appmenu/menuimporteradaptor.cpp
    appmenu/menuimporter.h
    appmenu/menuimporter.cpp
    appmenu/utils.cpp
    appmenu/utils_p.h
)

add_executable(${TARGET} ${SRCS} resources.qrc)
target_link_libraries(${TARGET}
    Qt5::DBus
    Qt5::Widgets
    Qt5::Concurrent
    Qt5::X11Extras
    KF5::DBusAddons
    KF5::WindowSystem
    ${X11_LIBRARIES}
    ${XCOMPOSITE_LDFLAGS}
    ${XDAMAGE_LIBRARIES}
    ${XRENDER_LIBRARIES}
    ${XCB_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}

)

install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
