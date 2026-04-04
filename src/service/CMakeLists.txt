set(LT_MODULE_SERVICE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/service.h
    ${CMAKE_CURRENT_SOURCE_DIR}/service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/daemon/daemon.h
    ${CMAKE_CURRENT_SOURCE_DIR}/workers/worker_process.h
    ${CMAKE_CURRENT_SOURCE_DIR}/workers/worker_process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/workers/worker_session.h
    ${CMAKE_CURRENT_SOURCE_DIR}/workers/worker_session.cpp
)

if (LT_WINDOWS)
    list(APPEND LT_MODULE_SERVICE_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/daemon/daemon_win.h
        ${CMAKE_CURRENT_SOURCE_DIR}/daemon/daemon_win.cpp
    )
elseif (LT_LINUX)
    list(APPEND LT_MODULE_SERVICE_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/daemon/daemon_linux.h
    )
endif()

add_library(lt_module_service STATIC
    ${LT_MODULE_SERVICE_SRCS}
)

source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${LT_MODULE_SERVICE_SRCS})

target_include_directories(lt_module_service
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lt_module_service
    PRIVATE
        lt_module_ltlib
        lt_module_audio
        lt_module_inputs
        lt_module_plat
        g3log
        protobuf::libprotobuf-lite
        ltproto
        rtc
        transport_api
)

if(LT_HAS_RTC2)
    target_link_libraries(lt_module_service PRIVATE rtc2)
endif()

set_code_analysis(lt_module_service ${LT_ENABLE_CODE_ANALYSIS})
