set(LT_MODULE_WORKER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/worker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/worker_check_decode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/worker_check_decode.cpp
)

if (LT_WINDOWS)
    list(APPEND LT_MODULE_WORKER_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/worker_streaming.h
        ${CMAKE_CURRENT_SOURCE_DIR}/worker_streaming.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/worker_setting.h
        ${CMAKE_CURRENT_SOURCE_DIR}/worker_setting.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/worker_clipboard.h
        ${CMAKE_CURRENT_SOURCE_DIR}/worker_clipboard.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/worker_check_dupl.h
        ${CMAKE_CURRENT_SOURCE_DIR}/worker_check_dupl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/display_setting.h
        ${CMAKE_CURRENT_SOURCE_DIR}/display_setting.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/session_change_observer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/session_change_observer.cpp
    )
endif()

add_library(lt_module_worker STATIC
    ${LT_MODULE_WORKER_SRCS}
)

source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${LT_MODULE_WORKER_SRCS})

target_include_directories(lt_module_worker
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lt_module_worker
    PRIVATE
        lt_module_ltlib
        lt_module_audio
        lt_module_inputs
        lt_module_plat
        g3log
        protobuf::libprotobuf-lite
        ltproto
        transport_api
        SDL2::SDL2-static
        ffmpeg
        Opus::opus
        amf
)

set_code_analysis(lt_module_worker ${LT_ENABLE_CODE_ANALYSIS})
