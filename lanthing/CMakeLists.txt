cmake_minimum_required(VERSION 3.20)
project(lanthing)

include(deploy_dlls)

#cmrc_add_resource_library(fonts
#    NAMESPACE fonts
#    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/NotoSansSC-Medium.ttf
#)

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
    # graphics->cepipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/cepipeline/video_capture_encode_pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/cepipeline/video_capture_encode_pipeline.cpp
    # graphics->decoder
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/video_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/video_decoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/ffmpeg_hard_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/ffmpeg_hard_decoder.cpp
    #${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/ffmpeg_soft_decoder.h
    #${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/decoder/ffmpeg_soft_decoder.cpp
    # graphics->renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/video_renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/video_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/d3d11_pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/d3d11_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/renderer_grab_inputs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/renderer/renderer_grab_inputs.cpp
    # graphics->drpipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/video_decode_render_pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/video_decode_render_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/ct_smoother.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/ct_smoother.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/gpu_capability.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/gpu_capability.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/video_statistics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/drpipeline/video_statistics.cpp
    # graphics->widget
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/widgets_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/widgets_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/control_bar_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/control_bar_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/statistics_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/statistics_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/status_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/widgets/status_widget.cpp

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/gamepad.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inputs/executor/gamepad.cpp

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
    ${CMAKE_CURRENT_SOURCE_DIR}/lanthing.rc
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

target_compile_definitions(${PROJECT_NAME} PRIVATE -DBUILDING_LT_EXE=1)

set_code_analysis(${PROJECT_NAME} ${LT_ENABLE_CODE_ANALYSIS})


if(LT_WINDOWS)
    set(PLATFORM_LIBS
            Secur32.lib dmoguids.lib d3d11.lib d3dcompiler.lib
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
    imgui
    ViGEmClient::ViGEmClientShared
    ltlib
    ltproto
    rtc
    transport_api
    transport
    #fonts
)

install(TARGETS ${PROJECT_NAME})
install(
    FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
    DESTINATION ${CMAKE_INSTALL_PREFIX}/pdb
)

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
