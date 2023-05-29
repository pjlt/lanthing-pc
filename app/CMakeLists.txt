cmake_minimum_required(VERSION 3.20)
project(app)


if(NOT DEFINED LT_QT_CMAKE_PATH)
    message(FATAL_ERROR "LT_QT_CMAKE_PATH not set!")
endif()
list(APPEND CMAKE_PREFIX_PATH ${LT_QT_CMAKE_PATH})

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)
qt_standard_project_setup()
get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")


set(APP_SRCS
    # root
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client_session.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client_session.cpp
    # views
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/mainwindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/mainwindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/link/mainpage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/link/mainpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/menu/menu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/menu/menu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/setting/settingpage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/setting/settingpage.cpp
)

set(APP_UI_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/mainwindow.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/link/mainpage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/menu/menu.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/setting/settingpage.ui

)

qt6_add_executable(${PROJECT_NAME}
    ${APP_SRCS}
    ${APP_UI_SRCS}
)

qt_add_resources(${PROJECT_NAME} "icons"
    PREFIX
        "./icons"
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/link.png
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/lock.png
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/pc.png
        ${CMAKE_CURRENT_SOURCE_DIR}/icons/user.png
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Widgets
        Qt6::Gui
        g3log
        protobuf::libprotobuf-lite
        breakpad
        ltlib
        ltproto
        rtc
)

install(TARGETS ${PROJECT_NAME})
install(CODE "execute_process(COMMAND ${WINDEPLOYQT_EXECUTABLE} ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${PROJECT_NAME}.exe)")