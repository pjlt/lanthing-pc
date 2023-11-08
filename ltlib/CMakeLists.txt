cmake_minimum_required(VERSION 3.21)
project(ltlib)

if (LT_WINDOWS)
    set(PLAT_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/win_service.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win_service.cpp
    )
    set(PLAT_LIBS
        winmm.lib
    )
elseif (LT_LINUX)
    set(PLAT_SRCS)
    set(PLAT_LIBS)
else()
    set(PLAT_SRCS)
    set(PLAT_LIBS)
endif()


set(LTLIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/ltlib.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/strings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/system.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/load_library.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/threads.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/times.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/spin_mutex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/reconnect_interval.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/settings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/time_sync.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/logging.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/singleton_process.h

    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/io/client.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/io/server.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/io/ioloop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/io/types.h


    ${CMAKE_CURRENT_SOURCE_DIR}/src/strings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/load_library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/threads.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/times.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/reconnect_interval.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/time_sync.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logging.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/singleton_process.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/ioloop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/client_secure_layer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/client_secure_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/client_transport_layer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/client_transport_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/server_transport_layer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io/server_transport_layer.cpp
)

if (LT_WINDOWS)
    list(APPEND LTLIB_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/win_service.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win_service.cpp
    )
endif()

add_library(${PROJECT_NAME} SHARED
    ${LTLIB_SRCS}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${LTLIB_SRCS})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(LT_WINDOWS)
set(PLATFORM_LIBS)
elseif(LT_LINUX)
set(PLATFORM_LIBS PkgConfig::X11)
else()
set(PLATFORM_LIBS)
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${PLAT_LIBS}
        utf8cpp
        sqlite3
        protobuf::libprotobuf-lite
        uv
        tomlpp
        g3log
        MbedTLS::mbedtls
        MbedTLS::mbedcrypto
        MbedTLS::mbedx509
        ltproto
        ${PLATFORM_LIBS}
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        BUILDING_LTLIB
)

set_code_analysis(${PROJECT_NAME} ${LT_ENABLE_CODE_ANALYSIS})

install(
	TARGETS ${PROJECT_NAME}
)

if (LT_WINDOWS)
    install(
        FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
        DESTINATION ${CMAKE_INSTALL_PREFIX}/pdb
    )
endif()

if(${LT_ENABLE_TEST})
if (LT_LINUX)
set(PLAT_LIBS m)
else()
set(PLAT_LIBS)
endif()
add_executable(test_settings
    ${CMAKE_CURRENT_SOURCE_DIR}/src/settings_tests.cpp
)
target_link_libraries(test_settings
    g3log
    sqlite3
    GTest::gtest
    GTest::gtest_main
    ${PROJECT_NAME}
    ${PLAT_LIBS}
)
add_test(NAME test_settings COMMAND test_settings)

endif() # if(${LT_ENABLE_TEST})