set(PLUGIN "datetime")
project(${PLUGIN})

set(SRCS
    datetime.h
    datetime.cpp
    datetimewidget.h
    datetimewidget.cpp
)

add_definitions("${QT_DEFINITIONS} -DQT_PLUGIN")
add_library(${PLUGIN} SHARED ${SRCS})

target_link_libraries(${PLUGIN} PRIVATE
    ${Qt5Widgets_LIBRARIES}
)
target_include_directories(${PLUGIN} PUBLIC ../interfaces)

install(TARGETS ${PLUGIN} LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/panda-statusbar/plugins)
