set(TARGET Menu)

set(SRCS
    main.cpp
    mainwindow.h
    mainwindow.cpp
    mainwidget.h
    mainwidget.cpp
    extensionwidget.h
    extensionwidget.cpp
    blurwindow.h
    blurwindow.cpp
    popupwindow.h
    popupwindow.cpp
    pluginmanager.h
    pluginmanager.cpp
    applicationinfo.h
    applicationinfo.cpp
    appmenuwidget.cpp
    appmenuwidget.h
    appmenu/appmenumodel.h
    appmenu/appmenumodel.cpp
#appmenu / dbusmenuimporter.h
#appmenu / dbusmenuimporter.cpp
#appmenu / dbusmenu_interface.h
#appmenu / dbusmenu_interface.cpp
#appmenu / dbusmenushortcut_p.h
#appmenu / dbusmenushortcut_p.cpp
#appmenu / dbusmenutypes_p.h
#appmenu / dbusmenutypes_p.cpp
    appmenu/menuimporteradaptor.h
    appmenu/menuimporteradaptor.cpp
    appmenu/menuimporter.h
    appmenu/menuimporter.cpp
    appmenu/utils.cpp
    appmenu/utils_p.h
    qtsingleapplication/qtsingleapplication.h
    qtsingleapplication/qtsingleapplication.cpp
    qtsingleapplication/qtlocalpeer.cpp
    thumbnails.cpp
)

#Create TS_FILES variable that contains all.ts files
file(GLOB TS_FILES translations/*.ts)

# Create/update .ts files from the source code
qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
# message(FATAL_ERROR "${QM_FILES}")

# Compile .ts to .qm
qt5_add_translation(TRANSLATION_QM ${TS_FILES})

add_custom_target(translations_update DEPENDS ${QM_FILES})

add_custom_target(translations ALL DEPENDS ${TRANSLATION_QM})

add_custom_target(resources ALL DEPENDS ${RESOURCE_FILES})

# add_dependencies(resources translations)

add_executable(${TARGET} ${SRCS} resources.qrc)

add_dependencies(${CMAKE_PROJECT_NAME} resources)
find_package(PkgConfig REQUIRED)
find_package(Qt5 COMPONENTS Network REQUIRED)  # probono: For qtsingleapplication
find_package(dbusmenu-qt5 REQUIRED)
find_package(KF5GlobalAccel REQUIRED)
pkg_check_modules(QALCULATE REQUIRED libqalculate)
target_include_directories(${TARGET} PUBLIC ${dbusmenu-qt5_INCLUDE_DIRS})

target_link_libraries(${TARGET}
    Qt5::DBus
    Qt5::Widgets
    Qt5::Concurrent
    Qt5::X11Extras
    Qt5::Network # probono: For qtsingleapplication
    KF5::DBusAddons
    KF5::WindowSystem
    KF5::Baloo
    KF5::GlobalAccel
    ${X11_LIBRARIES}
    ${XCOMPOSITE_LDFLAGS}
    ${XDAMAGE_LIBRARIES}
    ${XRENDER_LIBRARIES}
    ${XCB_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    ${QALCULATE_LIBRARIES}
    dbusmenu-qt5
)
if("${CMAKE_SYSTEM_NAME}" MATCHES "FreeBSD")
    target_link_libraries(${TARGET} procstat)
endif()

if("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
    target_link_libraries(${TARGET} procps)
endif()
install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/Resources/translations")

