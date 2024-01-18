cmake_minimum_required(VERSION 3.21)
project(lanthing)

include(deploy_dlls)

#cmrc_add_resource_library(fonts
#    NAMESPACE fonts
#    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/NotoSansSC-Medium.ttf
#)

set(LT_SVC_SRCS
    # Service
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/service.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/daemon/daemon.h
    # Service->workers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_process.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_session.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_session.cpp
)

set(LT_WORKER_WIN_SRCS
    # Worker
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker_streaming.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker_streaming.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker_setting.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker_setting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker_clipboard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker_clipboard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/display_setting.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/display_setting.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/session_change_observer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/session_change_observer.cpp
)

set(LT_WORKER_SRCS
    # Worker
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker_check_decode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker_check_decode.cpp
)

set(LT_VIDEO_ENCODER_SRCS
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
)

set(LT_VIDEO_CAPTURER_SRCS
    # graphics->capturer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/video_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/video_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi_video_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi_video_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi/duplication_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi/duplication_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi/common_types.h
)

set(LT_VIDEO_CE_PIPELINE_SRCS
    # graphics->cepipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/cepipeline/video_capture_encode_pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/cepipeline/video_capture_encode_pipeline.cpp
)

set(LT_VIDEO_DECODER_SRCS
    # graphics->decoder
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/video_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/video_decoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/ffmpeg_hard_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/ffmpeg_hard_decoder.cpp
    #${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/ffmpeg_soft_decoder.h
    #${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/ffmpeg_soft_decoder.cpp
)

set(LT_VIDEO_RENDERER_SRCS
    # graphics->renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/video_renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/video_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/renderer_grab_inputs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/renderer_grab_inputs.cpp
)
if (LT_WINDOWS)
    list(APPEND LT_VIDEO_RENDERER_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/d3d11_pipeline.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/d3d11_pipeline.cpp
    )
elseif (LT_LINUX)
    list(APPEND LT_VIDEO_RENDERER_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/va_gl_pipeline.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/va_gl_pipeline.cpp
    )
endif()

set(LT_VIDEO_DR_PIPELINE_SRCS
    # graphics->drpipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/video_decode_render_pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/video_decode_render_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/ct_smoother.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/ct_smoother.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/gpu_capability.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/gpu_capability.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/video_statistics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/video_statistics.cpp
)


set(LT_VIDEO_WIDGET_SRCS
    # graphics->widget
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/widgets_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/widgets_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/control_bar_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/control_bar_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/statistics_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/statistics_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/status_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/status_widget.cpp
)

set(LT_AUDIO_CAPTURER_SRCS
    # audio->capturer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/audio_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/audio_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/win_audio_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/win_audio_capturer.cpp
)

set(LT_AUDIO_PLAYER_SRCS
    # audio->player
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/player/audio_player.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/player/audio_player.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/player/sdl_audio_player.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/player/sdl_audio_player.cpp
)

set(LT_INPUT_CAPTURER_SRCS
    # input->capturer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/capturer/input_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/capturer/input_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/capturer/input_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/capturer/input_event.cpp
)

set(LT_INPUT_EXECUTOR_SRCS
    # input->executor
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/input_executor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/input_executor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/win_send_input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/win_send_input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/win_touch_input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/win_touch_input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/gamepad.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/gamepad.cpp
)

set(LT_CLIENT_SRCS
    # Client
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client.cpp
)

set(LT_PLAT_SRCS
    # platforms
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platforms/pc_sdl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platforms/pc_sdl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platforms/pc_sdl_input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platforms/pc_sdl_input.cpp
)

if(LT_WINDOWS)
    set(LT_DAEMON_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/service/daemon/daemon_win.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/service/daemon/daemon_win.cpp
    )
elseif(LT_LINUX)
    set(LT_DAEMON_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/service/daemon/daemon_linux.h
    )
else()
    set(LT_DAEMON_SRCS)
endif()

set(LT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lt_constants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/firewall.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/firewall.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/message_handler.h

    ${LT_WORKER_SRCS}

    ${LT_DAEMON_SRCS}

    ${LT_CLIENT_SRCS}
    ${LT_PLAT_SRCS}
    
    ${LT_VIDEO_DR_PIPELINE_SRCS}
    ${LT_VIDEO_DECODER_SRCS}
    ${LT_VIDEO_RENDERER_SRCS}
    ${LT_VIDEO_WIDGET_SRCS}
    ${LT_AUDIO_PLAYER_SRCS}
    ${LT_INPUT_CAPTURER_SRCS}
)

if (LT_WINDOWS)
    list(APPEND LT_SRCS
        ${LT_SVC_SRCS}
        ${LT_WORKER_WIN_SRCS}
        ${LT_VIDEO_ENCODER_SRCS}
        ${LT_VIDEO_CAPTURER_SRCS}
        ${LT_VIDEO_CE_PIPELINE_SRCS}
        ${LT_AUDIO_CAPTURER_SRCS}
        ${LT_INPUT_EXECUTOR_SRCS}
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/lanthing.rc.in
        ${CMAKE_CURRENT_SOURCE_DIR}/lanthing.rc
        @ONLY)
endif()

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/lanthing.rc
    ${LT_SRCS}
)

# 按照原始目录结构展开
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${LT_SRCS} ${PLATFORM_SRCS})

target_include_directories(${PROJECT_NAME}
    #让本项目的代码可以从"src"文件夹开始include
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE -DBUILDING_LT_EXE=1)

set_code_analysis(${PROJECT_NAME} ${LT_ENABLE_CODE_ANALYSIS})


if(LT_WINDOWS)
    set(PLATFORM_LIBS
            Secur32.lib dmoguids.lib d3d11.lib d3dcompiler.lib
            dxgi.lib Msdmo.lib Dxva2.lib winmm.lib wmcodecdspuuid.lib
            Dwmapi.lib Mfplat.lib Bcrypt.lib Mfuuid.lib Strmiids.lib
            ViGEmClient::ViGEmClientShared
    )
elseif(LT_LINUX)
    set(PLATFORM_LIBS
        m
        stdc++
        PkgConfig::X11
        PkgConfig::Va
        PkgConfig::Drm
        PkgConfig::Va-Drm
        PkgConfig::GL
        PkgConfig::EGL
        PkgConfig::GLib
        PkgConfig::MFX
    )
else()
    set(PLATFORM_LIBS)
endif()

target_link_libraries(${PROJECT_NAME}
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
    imgui
    ltlib
    ltproto
    rtc
    transport_api
    transport
    #fonts
    ${PLATFORM_LIBS}
)

install(TARGETS ${PROJECT_NAME})
if (LT_WINDOWS)
    install(
        FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
        DESTINATION ${CMAKE_INSTALL_PREFIX}/pdb
    )
endif()

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

if (LT_WINDOWS)
deploy_dlls(${PROJECT_NAME})
endif(LT_WINDOWS)

# 设置VS调试路径
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
