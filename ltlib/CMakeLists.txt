cmake_minimum_required(VERSION 3.20)
project(ltlib)


set(LTLIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/ltlib.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/win_service.h
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

    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/io/client.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/io/server.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/io/ioloop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltlib/io/types.h



    ${CMAKE_CURRENT_SOURCE_DIR}/src/win_service.cpp
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

add_library(${PROJECT_NAME} SHARED
    ${LTLIB_SRCS}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${LTLIB_SRCS})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
     	winmm.lib
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
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        BUILDING_LTLIB
)

install(
	TARGETS ${PROJECT_NAME}
)