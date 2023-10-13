cmake_minimum_required(VERSION 3.20)
project(app)

set(CMAKE_AUTORCC ON)

include (deploy_dlls)
include (deploy_qt6)

list(APPEND CMAKE_PREFIX_PATH ${LT_QT_CMAKE_PATH})

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui LinguistTools)
qt_standard_project_setup()
get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")

set(APP_SRCS
    # root
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client_session.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client_session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client_manager.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/service_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/service_manager.cpp

    # gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gui.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/mainwindow/mainwindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/mainwindow/mainwindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/link/mainpage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/link/mainpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/menu/menu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/menu/menu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/setting/settingpage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/setting/settingpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/components/progress_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/components/progress_widget.cpp
)

set(APP_UI_VIEWS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/mainwindow/mainwindow.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/link/mainpage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/menu/menu.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/setting/settingpage.ui
)

add_executable(${PROJECT_NAME} 
    ${APP_SRCS} 
    ${APP_UI_VIEWS} 
    # VS安装QT扩展工具可以直接编辑
    ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc)

qt_add_translations(${PROJECT_NAME}
    TS_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/i18n/lt-zh_CN.ts
)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_lupdate)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${APP_SRCS} ${APP_UI_VIEWS})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_code_analysis(${PROJECT_NAME} ${LT_ENABLE_CODE_ANALYSIS})

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Widgets
        Qt6::Gui
        g3log
        protobuf::libprotobuf-lite
        breakpad
        ltlib
        transport
        ltproto
        rtc
)

deploy_dlls(${PROJECT_NAME})
deploy_qt6(${PROJECT_NAME} ${WINDEPLOYQT_EXECUTABLE})

install(TARGETS ${PROJECT_NAME})
install(CODE "execute_process(COMMAND ${WINDEPLOYQT_EXECUTABLE} --no-translations --no-system-d3d-compiler --no-quick-import --pdb ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${PROJECT_NAME}.exe)")

# 设置VS调试路径
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
