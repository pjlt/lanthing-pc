set(LT_MODULE_LTLIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/strings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/strings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/system.h
    ${CMAKE_CURRENT_SOURCE_DIR}/event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/load_library.h
    ${CMAKE_CURRENT_SOURCE_DIR}/threads.h
    ${CMAKE_CURRENT_SOURCE_DIR}/threads.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/times.h
    ${CMAKE_CURRENT_SOURCE_DIR}/times.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spin_mutex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/reconnect_interval.h
    ${CMAKE_CURRENT_SOURCE_DIR}/reconnect_interval.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/settings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/time_sync.h
    ${CMAKE_CURRENT_SOURCE_DIR}/time_sync.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/logging.h
    ${CMAKE_CURRENT_SOURCE_DIR}/logging.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/singleton_process.h
    ${CMAKE_CURRENT_SOURCE_DIR}/transform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/transform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/versions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/client.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/server.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/ioloop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/ioloop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/client_secure_layer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/client_secure_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/client_transport_layer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/client_transport_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/client_transport_error.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/server_transport_layer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/server_transport_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/picohttpparser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/io/picohttpparser.c
)

if (LT_WINDOWS)
    list(APPEND LT_MODULE_LTLIB_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/system_win.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/event_win.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/load_library_win.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/singleton_process_win.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/thread_name_win.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/times_platform_win.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/client_transport_error_win.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/win_service.h
        ${CMAKE_CURRENT_SOURCE_DIR}/win_service.cpp
    )
else()
    list(APPEND LT_MODULE_LTLIB_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/thread_name_posix.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/times_platform_posix.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/event_stub.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/load_library_posix.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/singleton_process_posix.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/io/client_transport_error_posix.cpp
    )
endif()

if (LT_LINUX)
    list(APPEND LT_MODULE_LTLIB_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/system_linux.cpp
    )
elseif (LT_MAC)
    list(APPEND LT_MODULE_LTLIB_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/system_mac.cpp
    )
endif()

add_library(lt_module_ltlib STATIC
    ${LT_MODULE_LTLIB_SRCS}
)

source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${LT_MODULE_LTLIB_SRCS})

target_include_directories(lt_module_ltlib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

if (LT_LINUX)
    set(LT_MODULE_LTLIB_PLATFORM_LIBS PkgConfig::X11)
else()
    set(LT_MODULE_LTLIB_PLATFORM_LIBS)
endif()

target_link_libraries(lt_module_ltlib
    PRIVATE
        utf8cpp
        sqlite3
        protobuf::libprotobuf-lite
        uv_a
        tomlpp
        g3log
        MbedTLS::mbedtls
        MbedTLS::mbedcrypto
        MbedTLS::mbedx509
        ltproto
        ${LT_MODULE_LTLIB_PLATFORM_LIBS}
)

set_code_analysis(lt_module_ltlib ${LT_ENABLE_CODE_ANALYSIS})

if (${LT_ENABLE_TEST})
    if (LT_LINUX)
        set(LT_MODULE_LTLIB_TEST_PLAT_LIBS m)
    else()
        set(LT_MODULE_LTLIB_TEST_PLAT_LIBS)
    endif()

    add_executable(test_settings
        ${CMAKE_CURRENT_SOURCE_DIR}/settings_tests.cpp
    )
    target_link_libraries(test_settings
        g3log
        sqlite3
        GTest::gtest
        GTest::gtest_main
        lt_module_ltlib
        ${LT_MODULE_LTLIB_TEST_PLAT_LIBS}
    )
    add_test(NAME test_settings COMMAND test_settings)
endif()