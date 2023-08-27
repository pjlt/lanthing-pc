cmake_minimum_required(VERSION 3.20)
project(lanthing)

include(deploy_dlls)

set(LT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/string_keys.h
    #${CMAKE_CURRENT_SOURCE_DIR}/src/string_keys.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/firewall.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/firewall.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/message_handler.h

    # Client
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client.cpp

    # Service
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/service.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/daemon/daemon.h
    # Service->workers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_process.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_session.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_session.cpp

    # Worker
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/display_setting.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/display_setting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/session_change_observer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/session_change_observer.cpp

    # graphics->encoder
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/video_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/video_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/nvidia_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/nvidia_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/intel_allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/intel_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/intel_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/intel_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/amd_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/amd_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/params_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/encoder/params_helper.cpp
    # graphics->capturer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/video_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/video_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi_video_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi_video_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi/duplication_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi/duplication_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi/common_types.h
    # graphics->decoder
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/ct_smoother.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/ct_smoother.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/d3d11_pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/d3d11_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/gpu_capability.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/gpu_capability.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/pixel_shader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/vertex_shader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/video_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/video_decoder.cpp

    # audio->capturer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/audio_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/audio_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/win_audio_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/win_audio_capturer.cpp
    # audio->player
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/player/audio_player.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/player/audio_player.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/player/sdl_audio_player.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/player/sdl_audio_player.cpp

    # input->capturer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/capturer/input_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/capturer/input_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/capturer/input_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/capturer/input_event.cpp
    # input->executor
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/input_executor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/input_executor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/win_send_input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/win_send_input.cpp

    # platforms
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platforms/pc_sdl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platforms/pc_sdl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platforms/pc_sdl_input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platforms/pc_sdl_input.cpp
)

if(LT_WINDOWS)
    set(PLATFORM_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/service/daemon/daemon_win.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/service/daemon/daemon_win.cpp
    )
elseif(LT_LINUX)
    set(PLATFORM_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/service/daemon/daemon_linux.h
    )
else()
    set(PLATFORM_SRCS)
endif()

add_executable(${PROJECT_NAME}
    ${LT_SRCS}
    ${PLATFORM_SRCS}
)

# 按照原始目录结构展开
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${LT_SRCS} ${PLATFORM_SRCS})

target_include_directories(${PROJECT_NAME}
    #让本项目的代码可以从"src"文件夹开始include
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)


if(LT_WINDOWS)
    set(PLATFORM_LIBS
            Secur32.lib dmoguids.lib d3d11.lib d3d9.lib
            dxgi.lib Msdmo.lib Dxva2.lib winmm.lib wmcodecdspuuid.lib
            Dwmapi.lib Mfplat.lib Bcrypt.lib Mfuuid.lib Strmiids.lib)
else()
    set(PLATFORM_LIBS)
endif()

target_link_libraries(${PROJECT_NAME}
    ${PLATFORM_LIBS}
    g3log
    protobuf::libprotobuf-lite
    SDL2::SDL2
    ffmpeg
    Opus::opus
    nvcodec
    VPL::dispatcher
    amf
    uv
    breakpad
    ltlib
    ltproto
    rtc
    transport
)

install(TARGETS ${PROJECT_NAME})

install(CODE [[
    file(GET_RUNTIME_DEPENDENCIES
        RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPS
        UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPS
        EXECUTABLES $<TARGET_FILE:lanthing>
        DIRECTORIES ${CMAKE_SOURCE_DIR}
        PRE_INCLUDE_REGEXES ${CMAKE_SOURCE_DIR}
        POST_INCLUDE_REGEXES ${CMAKE_SOURCE_DIR}
        PRE_EXCLUDE_REGEXES "system32"
        POST_EXCLUDE_REGEXES "system32"
    )
    foreach(DEP_LIB ${RESOLVED_DEPS})
        file(INSTALL ${DEP_LIB} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
    endforeach()
]])

deploy_dlls(${PROJECT_NAME})

# 设置VS调试路径
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
