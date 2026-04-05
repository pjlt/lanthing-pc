file(GLOB_RECURSE LT_APP_UI_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/views/*.ui
)
list(REMOVE_ITEM LT_APP_UI_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window.ui
)
if (LT_APP_UI_FILES)
    message(FATAL_ERROR "New Qt .ui files are forbidden. Build UI in C++ code instead.")
endif()

set(LT_MODULE_APP_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/app.h
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/check_decode_ability.h
    ${CMAKE_CURRENT_SOURCE_DIR}/check_decode_ability.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/client/client_session.h
    ${CMAKE_CURRENT_SOURCE_DIR}/client/client_session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/client/client_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/client/client_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/service/service_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/service/service_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/views/gui.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/gui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/views/friendly_error_code.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/friendly_error_code.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_navigator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_navigator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_status_presenter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_status_presenter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_actions_binder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_actions_binder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_manager_page.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_manager_page.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_about_page.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_about_page.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_link_page.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_link_page.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_settings_page.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_settings_page.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/views/main_window/main_window_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/components/progress_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/components/progress_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/views/components/clickable_label.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/components/access_token_validator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/views/components/access_token_validator.cpp
)

if (LT_WINDOWS)
    list(APPEND LT_MODULE_APP_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/select_gpu.h
        ${CMAKE_CURRENT_SOURCE_DIR}/select_gpu.cpp
    )
endif()

list(FIND LT_MODULE_APP_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc LT_APP_RESOURCES_INDEX)
if (LT_APP_RESOURCES_INDEX EQUAL -1)
    message(FATAL_ERROR "resources.qrc must stay in lt_module_app sources for UI resources")
endif()

add_library(lt_module_app STATIC
    ${LT_MODULE_APP_SRCS}
)

source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${LT_MODULE_APP_SRCS})

target_include_directories(lt_module_app
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lt_module_app
    PRIVATE
        lt_build_config
        lt_module_ltlib
        g3log
        protobuf::libprotobuf-lite
        ltproto
        transport_api
        Qt6::Widgets
        Qt6::Gui
)

if (LT_WINDOWS)
    target_link_libraries(lt_module_app
        PRIVATE
            wintoastlib
    )
endif()

set_code_analysis(lt_module_app ${LT_ENABLE_CODE_ANALYSIS})

if (LT_ENABLE_TEST AND BUILD_TESTING)
    add_executable(test_friendly_error_code
        ${CMAKE_CURRENT_SOURCE_DIR}/views/friendly_error_code_tests.cpp
    )
    target_link_libraries(test_friendly_error_code
        GTest::gtest
        GTest::gtest_main
        lt_module_app
        ltproto
        protobuf::libprotobuf-lite
        Qt6::Widgets
        Qt6::Gui
    )
    add_test(NAME test_friendly_error_code COMMAND test_friendly_error_code)

    add_executable(test_access_token_validator
        ${CMAKE_CURRENT_SOURCE_DIR}/views/components/access_token_validator_tests.cpp
    )
    target_link_libraries(test_access_token_validator
        GTest::gtest
        GTest::gtest_main
        lt_module_app
        Qt6::Widgets
        Qt6::Gui
    )
    add_test(NAME test_access_token_validator COMMAND test_access_token_validator)
endif()
