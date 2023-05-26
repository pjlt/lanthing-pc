cmake_minimum_required(VERSION 3.20)
project(lanthing-pc)

set(CMAKE_CXX_STANDARD 17)
include(lanthing-platforms-check.cmake)

# ƒ¨»œ±‡“ÎRelease
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(LT_WINDOWS)
    #  π”√MT,MTd
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_definitions(UNICODE _UNICODE WIN32 _WIN32 NOMINMAX WIN32_LEAN_AND_MEAN)# NOGDI)
    add_compile_options(/wd4819)
    #add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()


if(NOT LT_SERVER_ADDR)
    set(LT_SERVER_ADDR lanthing.net)
endif()

option(protobuf_BUILD_TESTS OFF)
option(G3_SHARED_LIB OFF)

add_subdirectory(third_party/g3log)
add_subdirectory(third_party/protobuf/cmake)
add_subdirectory(third_party/tomlplusplus)
add_subdirectory(third_party/utfcpp)
add_subdirectory(third_party/libuv)
add_subdirectory(third_party/mbedtls)
add_subdirectory(third_party/nvcodec)
add_subdirectory(third_party/mfx_builder)
add_subdirectory(third_party/SDL)
add_subdirectory(third_party/ffmpeg)

add_subdirectory(ltrtc)
add_subdirectory(ltproto)
add_subdirectory(ltlib)
add_subdirectory(lanthing)
add_subdirectory(lanthing-qt)