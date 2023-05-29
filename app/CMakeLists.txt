cmake_minimum_required(VERSION 3.20)
project(app)

# QT 需要自动生成一些文件(仍旧需要)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include (deploy_dlls)
include (deploy_qt6)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)
prepare_qt6()

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

set(APP_UI_VIEWS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/mainwindow.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/link/mainpage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/menu/menu.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/setting/settingpage.ui
)

add_executable(${PROJECT_NAME} 
    ${APP_SRCS} 
    ${APP_UI_VIEWS} 
    # VS安装QT扩展工具可以直接编辑
    ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${APP_SRCS} ${APP_UI_VIEWS})

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
deploy_dlls(${PROJECT_NAME})
deploy_qt6(${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME})
install(CODE "execute_process(COMMAND ${WINDEPLOYQT_EXECUTABLE} ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${PROJECT_NAME}.exe)")

# 设置VS调试路径
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
