cmake_minimum_required(VERSION 3.0)

project(Menu)

set(CMAKE_INSTALL_PREFIX "/System/Menu.app")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# include(GNUInstallDirs)

# Do not put qDebug() into Release builds
if(NOT CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(PkgConfig REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5X11Extras REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(Qt5LinguistTools REQUIRED)
find_package(KF5DBusAddons REQUIRED)
find_package(KF5WindowSystem REQUIRED)
find_package(KF5Baloo REQUIRED)
find_package(XCB REQUIRED COMPONENTS xcb xcb-util xcb-damage)
find_package(X11 REQUIRED)
find_package(PulseAudio)

pkg_check_modules(XCOMPOSITE REQUIRED xcomposite)
pkg_check_modules(XDAMAGE REQUIRED xdamage)
pkg_check_modules(XRENDER REQUIRED xrender)
pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh x11)

file(GLOB INTERFACES "interfaces/*.h")

add_subdirectory(src)
add_subdirectory(plugin-datetime)
add_subdirectory(plugin-statusnotifier)
add_subdirectory(plugin-volume)
add_subdirectory(plugin-windows)
# add_subdirectory(plugin-battery)
add_subdirectory(Shutdown)

install(FILES ${INTERFACES} DESTINATION ${CMAKE_INSTALL_PREFIX}/Resources/include/menubar)

install(FILES Menu.png DESTINATION ${CMAKE_INSTALL_PREFIX}/Resources)
