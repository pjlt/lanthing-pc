cmake_minimum_required(VERSION 3.20)
project(lanthing-pc)

set(CMAKE_CXX_STANDARD 17)
include(cmake/platforms-check.cmake)

# ƒ¨»œ±‡“ÎRelease
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(LT_WINDOWS)
    #  π”√MT,MTd
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_definitions(UNICODE _UNICODE WIN32 _WIN32 NOMINMAX WIN32_LEAN_AND_MEAN)# NOGDI)
    add_compile_options(/wd4819)
    #add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()


if(NOT LT_SERVER_ADDR)
    set(LT_SERVER_ADDR lanthing.net)
endif()

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    set(THIRD_PARTY_BUILD_TYPE Debug)
elseif(CMAKE_BUILD_TYPE STREQUAL Release OR CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
    set(THIRD_PARTY_BUILD_TYPE Release)
else()
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

add_subdirectory(third_party/g3log)
add_subdirectory(third_party/tomlplusplus)
add_subdirectory(third_party/utfcpp)
add_subdirectory(third_party/nvcodec)
add_subdirectory(third_party/mfx_builder)
add_subdirectory(third_party/ffmpeg)
add_subdirectory(third_party/breakpad_builder)

find_package(protobuf REQUIRED PATHS third_party/protobuf/build2/install/${THIRD_PARTY_BUILD_TYPE})
find_package(GTest REQUIRED PATHS third_party/googletest/build/install/${THIRD_PARTY_BUILD_TYPE})
#find_package(g3log REQUIRED PATHS third_party/g3log/build/install/${THIRD_PARTY_BUILD_TYPE})
find_package(MbedTLS REQUIRED PATHS third_party/mbedtls/build/install/${THIRD_PARTY_BUILD_TYPE})
find_package(libuv REQUIRED PATHS third_party/libuv/build/install/${THIRD_PARTY_BUILD_TYPE})
find_package(SDL2 REQUIRED PATHS third_party/SDL/build/install/${THIRD_PARTY_BUILD_TYPE})

add_subdirectory(rtc)
add_subdirectory(ltproto)
add_subdirectory(ltlib)
add_subdirectory(lanthing)
add_subdirectory(lanthing-qt)