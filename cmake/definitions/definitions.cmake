# 默认编译Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 主要是把RelWithDebInfo在名字上也变成Release，方便编写脚本
if (CMAKE_BUILD_TYPE STREQUAL Debug)
    set(LT_BUILD_TYPE Debug)
elseif(CMAKE_BUILD_TYPE STREQUAL Release OR CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
    set(LT_BUILD_TYPE Release)
else()
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

# 平台检测
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(LT_LINUX ON)
    set(LT_PLAT linux)
    set(LT_THIRD_POSTFIX ${LT_PLAT})
    set(LT_PLATFORM_DEFINITION LT_LINUX=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(LT_ANDROID ON)
    set(LT_PLAT android)
    set(LT_THIRD_POSTFIX ${LT_PLAT})
    set(LT_PLATFORM_DEFINITION LT_ANDROID=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(LT_MAC ON)
    set(LT_PLAT mac)
    set(LT_THIRD_POSTFIX ${LT_PLAT})
    set(LT_PLATFORM_DEFINITION LT_MAC=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
    set(LT_IOS ON)
    set(LT_PLAT ios)
    set(LT_THIRD_POSTFIX ${LT_PLAT})
    set(LT_PLATFORM_DEFINITION LT_IOS=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(LT_WINDOWS ON)
    set(LT_PLAT win)
    set(LT_THIRD_POSTFIX ${LT_PLAT}/${LT_BUILD_TYPE})
    set(LT_PLATFORM_DEFINITION LT_WINDOWS=1)
endif()

set(CMAKE_CXX_STANDARD 20)
set(LT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" describe --match=NeVeRmAtCh --always --abbrev=7 --dirty OUTPUT_VARIABLE LT_COMMIT_ID OUTPUT_STRIP_TRAILING_WHITESPACE)

if (EXISTS ${CMAKE_SOURCE_DIR}/src/transport/rtc2/CMakeLists.txt)
	set(LT_HAS_RTC2 ON)
endif()

if(${LT_ENABLE_TEST})
    include(CTest)
endif()

if(LT_WINDOWS)
    include(${CMAKE_CURRENT_LIST_DIR}/windows.cmake)
elseif(LT_MAC)
    include(${CMAKE_CURRENT_LIST_DIR}/mac.cmake)
endif()

add_library(lt_build_config INTERFACE)

target_compile_definitions(lt_build_config
    INTERFACE
        ${LT_PLATFORM_DEFINITION}
        LT_SERVER_ADDR=${LT_SERVER_ADDR}
        LT_SERVER_SVC_PORT=${LT_SERVER_SVC_PORT}
        LT_SERVER_APP_PORT=${LT_SERVER_APP_PORT}
        LT_SERVER_USE_SSL=$<IF:$<BOOL:${LT_SERVER_USE_SSL}>,true,false>
        LT_RUN_AS_SERVICE=$<IF:$<BOOL:${LT_RUN_AS_SERVICE}>,true,false>
        LT_WIN_SERVICE_NAME=${LT_WIN_SERVICE_NAME}
        LT_WIN_SERVICE_DISPLAY_NAME=${LT_WIN_SERVICE_DISPLAY_NAME}
        LT_VERSION_MAJOR=${LT_VERSION_MAJOR}
        LT_VERSION_MINOR=${LT_VERSION_MINOR}
        LT_VERSION_PATCH=${LT_VERSION_PATCH}
        LT_CRASH_ON_THREAD_HANGS=$<IF:$<BOOL:${LT_CRASH_ON_THREAD_HANGS}>,true,false>
        LT_ENABLE_SELF_CONNECT=$<IF:$<BOOL:${LT_ENABLE_SELF_CONNECT}>,true,false>
        LT_USE_PREBUILT_VIDEO2=$<IF:$<BOOL:${LT_USE_PREBUILT_VIDEO2}>,true,false>
        LT_DUMP=$<IF:$<BOOL:${LT_DUMP}>,true,false>
        LT_DUMP_URL=${LT_DUMP_URL}
        LT_COMMIT_ID=${LT_COMMIT_ID}
        $<$<BOOL:${LT_HAS_RTC2}>:LT_HAS_RTC2=1>
        ${LT_BUILD_CONFIG_PLATFORM_DEFINITIONS}
)

target_include_directories(lt_build_config
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/certs
)

target_compile_options(lt_build_config
    INTERFACE
        ${LT_BUILD_CONFIG_PLATFORM_OPTIONS}
)