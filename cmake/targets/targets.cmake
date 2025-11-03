set(LT_APP_SRCS
    # root
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/app.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/check_decode_ability.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/check_decode_ability.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/client/client_session.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/client/client_session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/client/client_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/client/client_manager.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/service/service_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/service/service_manager.cpp

    # gui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/resources.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/gui.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/gui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/friendly_error_code.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/friendly_error_code.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/mainwindow/mainwindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/mainwindow/mainwindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/components/progress_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/components/progress_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/components/clickable_label.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/components/access_token_validator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/components/access_token_validator.cpp
)

set(LT_APP_UI_VIEWS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/views/mainwindow/mainwindow.ui
)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker_check_dupl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/worker_check_dupl.cpp
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
    # video->encoder
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/video_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/video_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/nvidia_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/nvidia_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/intel_allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/intel_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/intel_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/intel_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/amd_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/amd_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/openh264_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/openh264_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/params_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/encoder/params_helper.cpp
)

set(LT_VIDEO_CAPTURER_SRCS
    # video->capturer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/capturer/video_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/capturer/video_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/capturer/dxgi_video_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/capturer/dxgi_video_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/capturer/dxgi/duplication_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/capturer/dxgi/duplication_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/capturer/dxgi/common_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/capturer/nvfbc_video_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/capturer/nvfbc_video_capturer.cpp
)

set(LT_VIDEO_CE_PIPELINE_SRCS
    # video->cepipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/cepipeline/video_capture_encode_pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/cepipeline/video_capture_encode_pipeline.cpp
)

set(LT_VIDEO_DECODER_SRCS
    # video->decoder
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/decoder/video_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/decoder/video_decoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/decoder/ffmpeg_hard_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/decoder/ffmpeg_hard_decoder.cpp
    #${CMAKE_CURRENT_SOURCE_DIR}/src/video/decoder/ffmpeg_soft_decoder.h
    #${CMAKE_CURRENT_SOURCE_DIR}/src/video/decoder/ffmpeg_soft_decoder.cpp
)

set(LT_VIDEO_RENDERER_SRCS
    # video->renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/video_renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/video_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/renderer_grab_inputs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/renderer_grab_inputs.cpp
)

set(LTLIB_SRCS
    # ltlib
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/strings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/strings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/system.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/load_library.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/load_library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/threads.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/threads.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/times.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/times.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/spin_mutex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/reconnect_interval.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/reconnect_interval.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/settings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/time_sync.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/time_sync.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/logging.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/logging.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/singleton_process.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/singleton_process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/transform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/transform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/versions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/client.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/server.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/ioloop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/ioloop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/client_secure_layer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/client_secure_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/client_transport_layer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/client_transport_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/server_transport_layer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/io/server_transport_layer.cpp
)

if (LT_WINDOWS)
    list(APPEND LT_VIDEO_RENDERER_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/d3d11_pipeline.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/d3d11_pipeline.cpp
    )
    list(APPEND LT_VIDEO_DECODER_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video/decoder/openh264_decoder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video/decoder/openh264_decoder.cpp
    )
    list(APPEND LT_APP_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/select_gpu.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/select_gpu.cpp
    )
    list(APPEND LTLIB_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/win_service.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib/win_service.cpp
    )
elseif (LT_LINUX)
    list(APPEND LT_VIDEO_RENDERER_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/va_gl_pipeline.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/va_gl_pipeline.cpp
    )
elseif (LT_MAC)
    list(APPEND LT_VIDEO_RENDERER_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/vtb_gl_pipeline.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/vtb_gl_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/vtb_gl_pipeline_plat.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video/renderer/vtb_gl_pipeline_plat.m
    )
endif()

set(LT_VIDEO_DR_PIPELINE_SRCS
    # video->drpipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/drpipeline/video_decode_render_pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/drpipeline/video_decode_render_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/drpipeline/ct_smoother.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/drpipeline/ct_smoother.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/drpipeline/gpu_capability.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/drpipeline/gpu_capability.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/drpipeline/video_statistics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/drpipeline/video_statistics.cpp
)


set(LT_VIDEO_WIDGET_SRCS
    # video->widget
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/widgets/widgets_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/widgets/widgets_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/widgets/control_bar_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/widgets/control_bar_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/widgets/statistics_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/widgets/statistics_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/widgets/status_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/video/widgets/status_widget.cpp
)

set(LT_AUDIO_CAPTURER_SRCS
    # audio->capturer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/audio_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/audio_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/win_audio_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/win_audio_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/fake_audio_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/capturer/fake_audio_capturer.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plat/pc_sdl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plat/pc_sdl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plat/pc_sdl_input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plat/pc_sdl_input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plat/video_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plat/video_device.cpp
)

if (LT_WINDOWS)
    list(APPEND LT_PLAT_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/plat/d3d11_video_device.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/plat/d3d11_video_device.cpp
    )
endif(LT_WINDOWS)

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

    ${LTLIB_SRCS}

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

    ${LT_APP_SRCS}
    ${LT_APP_UI_VIEWS}
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

    set(LT_LANTHING_RC ${CMAKE_CURRENT_SOURCE_DIR}/lanthing.rc)
endif()

add_executable(${PROJECT_NAME}
    ${LT_LANTHING_RC}
    ${LT_SRCS}
)

# 按照原始目录结构展开
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${LT_SRCS} ${PLATFORM_SRCS})

qt_add_translations(${PROJECT_NAME}
    TS_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/i18n/lt-zh_CN.ts
)

target_include_directories(${PROJECT_NAME}
    #让本项目的代码可以从"src"文件夹开始include
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE -DBUILDING_LT_EXE=1)
qt_disable_unicode_defines(${PROJECT_NAME})

set_code_analysis(${PROJECT_NAME} ${LT_ENABLE_CODE_ANALYSIS})

if(LT_WINDOWS)
    if(LT_USE_PREBUILT_VIDEO2)
        #add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/prebuilt/video2/${LT_THIRD_POSTFIX})
    else(LT_USE_PREBUILT_VIDEO2)
        #add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/video2)
    endif(LT_USE_PREBUILT_VIDEO2)
    set(PLATFORM_LIBS
            #video2
            openh264
            Secur32.lib dmoguids.lib d3d11.lib d3dcompiler.lib
            dxgi.lib Msdmo.lib Dxva2.lib winmm.lib wmcodecdspuuid.lib
            Dwmapi.lib Mfplat.lib Bcrypt.lib Mfuuid.lib Strmiids.lib
            ViGEmClient::ViGEmClientShared
            VPL::dispatcher nvcodec wintoastlib nbclipboard
    )
elseif(LT_LINUX)
    set(PLATFORM_LIBS
        m
        stdc++
        PkgConfig::X11
        PkgConfig::Va
        PkgConfig::Va-X11
        PkgConfig::Va-Wayland
        PkgConfig::Drm
        PkgConfig::Va-Drm
        PkgConfig::GL
        PkgConfig::EGL
        PkgConfig::GLib
        PkgConfig::MFX
        VPL::dispatcher
    )
elseif(LT_MAC)
    set(PLATFORM_LIBS
        OpenGL::GL
    )
    set(PLATFORM_LIBS "${PLATFORM_LIBS}" -lobjc ${LIB_CORE_VIDEO} ${LIB_APP_KIT} ${LIB_IO_SURFACE})
else()
    set(PLATFORM_LIBS)
endif()

set(TRANSPORT_LIBS rtc transport_api transport)
if(LT_HAS_RTC2)
    list(APPEND TRANSPORT_LIBS rtc2)
endif()

target_link_libraries(${PROJECT_NAME}
    Qt6::Widgets
    Qt6::Gui
    g3log
    protobuf::libprotobuf-lite
    SDL2::SDL2-static
    ffmpeg
    Opus::opus
    amf
    nvfbc
    uv_a
    breakpad
    imgui
    utf8cpp
    sqlite3
    tomlpp
    MbedTLS::mbedtls
    MbedTLS::mbedcrypto
    MbedTLS::mbedx509
    ltproto
    ${TRANSPORT_LIBS}
    #fonts
    ${PLATFORM_LIBS}
)

if (LT_WINDOWS)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
endif(LT_WINDOWS)

# 设置VS调试路径
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>")