cmake_minimum_required(VERSION 3.20)

project(lanthing-pc)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeRC.cmake)

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
    add_compile_definitions(LT_LINUX=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(LT_ANDROID ON)
    add_compile_definitions(LT_ANDROID=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(LT_MAC ON)
    add_compile_definitions(LT_MAC=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
set(LT_IOS ON)
    add_compile_definitions(LT_IOS=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
set(LT_WINDOWS ON)
    add_compile_definitions(LT_WINDOWS=1)
endif()


# 默认编译Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)

if(LT_WINDOWS)
    add_compile_definitions(WIN32 _WIN32 NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)# NOGDI)
    add_compile_options(/wd4819 /wd4251 /utf-8) #c4251这个警告跟dll导出有关，如果整个项目的编译平台、编译工具链、运行时都是一样的，理论上不会有问题
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
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
#misc
add_definitions(-DLT_CRASH_ON_THREAD_HANGS=$<IF:$<BOOL:${LT_CRASH_ON_THREAD_HANGS}>,true,false>)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/certs)


# 主要是把RelWithDebInfo等也变成Release，方便编写脚本
if (CMAKE_BUILD_TYPE STREQUAL Debug)
    set(THIRD_PARTY_BUILD_TYPE Debug)
elseif(CMAKE_BUILD_TYPE STREQUAL Release OR CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
    set(THIRD_PARTY_BUILD_TYPE Release)
else()
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

add_subdirectory(third_party/tomlplusplus)
add_subdirectory(third_party/utfcpp)
add_subdirectory(third_party/nvcodec)
add_subdirectory(third_party/amf)
add_subdirectory(third_party/prebuilt/ffmpeg)
add_subdirectory(third_party/breakpad_builder)
#add_subdirectory(third_party/lodepng)

#protobuf依赖absl
find_package(absl REQUIRED PATHS third_party/prebuilt/protobuf/${THIRD_PARTY_BUILD_TYPE}/lib/cmake)
find_package(protobuf REQUIRED PATHS third_party/prebuilt/protobuf/${THIRD_PARTY_BUILD_TYPE})
find_package(GTest REQUIRED PATHS third_party/prebuilt/googletest/${THIRD_PARTY_BUILD_TYPE})
find_package(g3log REQUIRED PATHS third_party/prebuilt/g3log/${THIRD_PARTY_BUILD_TYPE})
find_package(MbedTLS REQUIRED PATHS third_party/prebuilt/mbedtls/${THIRD_PARTY_BUILD_TYPE})
find_package(libuv REQUIRED PATHS third_party/prebuilt/libuv/${THIRD_PARTY_BUILD_TYPE})
find_package(SDL2 REQUIRED PATHS third_party/prebuilt/sdl/${THIRD_PARTY_BUILD_TYPE})
find_package(VPL REQUIRED PATHS third_party/prebuilt/onevpl/${THIRD_PARTY_BUILD_TYPE})
find_package(Opus REQUIRED PATHS third_party/prebuilt/opus/${THIRD_PARTY_BUILD_TYPE})
find_package(ViGEmClient REQUIRED PATHS third_party/prebuilt/vigemclient/${THIRD_PARTY_BUILD_TYPE})

# 依赖于SDL
add_subdirectory(third_party/imgui_builder)

add_subdirectory(ltproto)
add_subdirectory(ltlib)
add_subdirectory(transport)
add_subdirectory(lanthing)
add_subdirectory(app)
