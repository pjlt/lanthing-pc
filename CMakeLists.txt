cmake_minimum_required(VERSION 3.21)

project(lanthing-pc)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeRC.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/code_analysis.cmake)

set(LT_TRANSPORT_RTC 1)
set(LT_TRANSPORT_RTC2 2)
set(LT_TRANSPORT_TCP 3)
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/options-user.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/options-user.cmake)
else()
    include(${CMAKE_CURRENT_SOURCE_DIR}/options-default.cmake)
endif()

# 平台检测
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(LT_LINUX ON)
    set(LT_PLAT linux)
    add_compile_definitions(LT_LINUX=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(LT_ANDROID ON)
    set(LT_PLAT android)
    add_compile_definitions(LT_ANDROID=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(LT_MAC ON)
    set(LT_PLAT mac)
    add_compile_definitions(LT_MAC=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    set(LT_IOS ON)
    set(LT_PLAT ios)
    add_compile_definitions(LT_IOS=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(LT_WINDOWS ON)
    set(LT_PLAT win)
    add_compile_definitions(LT_WINDOWS=1)
endif()


# 默认编译Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
set(LT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(LT_WINDOWS)
    add_compile_definitions(WIN32 _WIN32 NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)# NOGDI)
    add_compile_options(/utf-8)
endif()


#服务器地址
add_definitions(-DLT_SERVER_ADDR=${LT_SERVER_ADDR})
add_definitions(-DLT_SERVER_SVC_PORT=${LT_SERVER_SVC_PORT})
add_definitions(-DLT_SERVER_APP_PORT=${LT_SERVER_APP_PORT})
add_definitions(-DLT_SERVER_USE_SSL=$<IF:$<BOOL:${LT_SERVER_USE_SSL}>,true,false>)
#windows服务
add_definitions(-DLT_RUN_AS_SERVICE=$<IF:$<BOOL:${LT_RUN_AS_SERVICE}>,true,false>)
add_definitions(-DLT_WIN_SERVICE_NAME=${LT_WIN_SERVICE_NAME})
add_definitions(-DLT_WIN_SERVICE_DISPLAY_NAME=${LT_WIN_SERVICE_DISPLAY_NAME})
#transport
add_definitions(-DLT_TRANSPORT_RTC=${LT_TRANSPORT_RTC})
add_definitions(-DLT_TRANSPORT_RTC2=${LT_TRANSPORT_RTC2})
add_definitions(-DLT_TRANSPORT_TCP=${LT_TRANSPORT_TCP})
add_definitions(-DLT_TRANSPORT_TYPE=${LT_TRANSPORT_TYPE})
#version
add_definitions(-DLT_VERSION_MAJOR=${LT_VERSION_MAJOR})
add_definitions(-DLT_VERSION_MINOR=${LT_VERSION_MINOR})
add_definitions(-DLT_VERSION_PATCH=${LT_VERSION_PATCH})
#misc
add_definitions(-DLT_CRASH_ON_THREAD_HANGS=$<IF:$<BOOL:${LT_CRASH_ON_THREAD_HANGS}>,true,false>)
add_definitions(-DLT_ENABLE_SELF_CONNECT=$<IF:$<BOOL:${LT_ENABLE_SELF_CONNECT}>,true,false>)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/certs)


# 主要是把RelWithDebInfo在名字上也变成Release，方便编写脚本
if (CMAKE_BUILD_TYPE STREQUAL Debug)
    set(LT_BUILD_TYPE Debug)
elseif(CMAKE_BUILD_TYPE STREQUAL Release OR CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
    set(LT_BUILD_TYPE Release)
else()
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

if(${LT_ENABLE_TEST})
    include(CTest)
endif()

if (LT_WINDOWS)
    set(LT_THIRD_POSTFIX ${LT_PLAT}/${LT_BUILD_TYPE})
else()
    set(LT_THIRD_POSTFIX ${LT_PLAT})
endif()

add_subdirectory(third_party/tomlplusplus)
add_subdirectory(third_party/utfcpp)
add_subdirectory(third_party/nvcodec)
add_subdirectory(third_party/amf)
add_subdirectory(third_party/prebuilt/ffmpeg/${LT_PLAT})
add_subdirectory(third_party/breakpad_builder)
#add_subdirectory(third_party/lodepng)
add_subdirectory(third_party/prebuilt/sqlite/${LT_PLAT})

set(protobuf_MODULE_COMPATIBLE ON)
#protobuf依赖absl
find_package(absl REQUIRED PATHS third_party/prebuilt/protobuf/${LT_THIRD_POSTFIX}/lib/cmake)
find_package(Protobuf REQUIRED PATHS third_party/prebuilt/protobuf/${LT_THIRD_POSTFIX})
find_package(GTest REQUIRED PATHS third_party/prebuilt/googletest/${LT_THIRD_POSTFIX})
find_package(g3log REQUIRED PATHS third_party/prebuilt/g3log/${LT_THIRD_POSTFIX})
find_package(MbedTLS REQUIRED PATHS third_party/prebuilt/mbedtls/${LT_THIRD_POSTFIX})
find_package(libuv REQUIRED PATHS third_party/prebuilt/libuv/${LT_THIRD_POSTFIX})
find_package(SDL2 REQUIRED PATHS third_party/prebuilt/sdl/${LT_THIRD_POSTFIX})
find_package(VPL REQUIRED PATHS third_party/prebuilt/onevpl/${LT_THIRD_POSTFIX})
find_package(Opus REQUIRED PATHS third_party/prebuilt/opus/${LT_THIRD_POSTFIX})

if (LT_WINDOWS)
    find_package(ViGEmClient REQUIRED PATHS third_party/prebuilt/vigemclient/${LT_THIRD_POSTFIX})
    add_subdirectory(third_party/wintoast)
endif(LT_WINDOWS)

if (LT_LINUX)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLib REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
pkg_check_modules(Va REQUIRED IMPORTED_TARGET libva)
pkg_check_modules(Drm REQUIRED IMPORTED_TARGET libdrm)
pkg_check_modules(Va-Drm REQUIRED IMPORTED_TARGET libva-drm)
pkg_check_modules(GL REQUIRED IMPORTED_TARGET gl)
pkg_check_modules(GLES REQUIRED IMPORTED_TARGET glesv2)
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)
endif(LT_LINUX)

# 依赖于SDL
add_subdirectory(third_party/imgui_builder)

add_subdirectory(ltproto)
add_subdirectory(ltlib)
add_subdirectory(transport)
add_subdirectory(lanthing)
add_subdirectory(app)
