set(LT_MODULE_INPUTS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/input_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/input_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/input_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/input_event.cpp
)

if (LT_WINDOWS)
    list(APPEND LT_MODULE_INPUTS_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/executor/input_executor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/executor/input_executor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/executor/win_send_input.h
        ${CMAKE_CURRENT_SOURCE_DIR}/executor/win_send_input.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/executor/win_touch_input.h
        ${CMAKE_CURRENT_SOURCE_DIR}/executor/win_touch_input.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/executor/gamepad.h
        ${CMAKE_CURRENT_SOURCE_DIR}/executor/gamepad.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/executor/scancode.h
    )
endif()

add_library(lt_module_inputs STATIC
    ${LT_MODULE_INPUTS_SRCS}
)

source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${LT_MODULE_INPUTS_SRCS})

target_include_directories(lt_module_inputs
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lt_module_inputs
    PRIVATE
    lt_build_config
        g3log
        protobuf::libprotobuf-lite
        ltproto
        transport_api
)

if (LT_WINDOWS)
    target_link_libraries(lt_module_inputs
        PRIVATE
            ViGEmClient::ViGEmClientShared
    )
endif()

set_code_analysis(lt_module_inputs ${LT_ENABLE_CODE_ANALYSIS})

if (LT_ENABLE_TEST AND BUILD_TESTING)
    add_executable(test_input_event
        ${CMAKE_CURRENT_SOURCE_DIR}/capturer/input_event_tests.cpp
    )
    target_link_libraries(test_input_event
        GTest::gtest
        GTest::gtest_main
        lt_module_inputs
    )
    add_test(NAME test_input_event COMMAND test_input_event)
endif()
