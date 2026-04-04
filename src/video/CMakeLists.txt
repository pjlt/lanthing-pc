set(LT_MODULE_VIDEO_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/video_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/video_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/dxgi_video_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/dxgi_video_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/dxgi/duplication_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/dxgi/duplication_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/dxgi/common_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cepipeline/video_capture_encode_pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cepipeline/video_capture_encode_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/video_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/video_decoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/ffmpeg_hard_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/ffmpeg_hard_decoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drpipeline/video_decode_render_pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drpipeline/video_decode_render_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drpipeline/ct_smoother.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drpipeline/ct_smoother.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drpipeline/gpu_capability.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drpipeline/gpu_capability.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drpipeline/video_statistics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drpipeline/video_statistics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/video_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/video_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/nvidia_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/nvidia_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/intel_allocator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/intel_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/intel_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/intel_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/amd_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/amd_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/openh264_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/openh264_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/params_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/encoder/params_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/video_renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/video_renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/renderer_grab_inputs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer/renderer_grab_inputs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/widgets_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/widgets_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/control_bar_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/control_bar_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/statistics_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/statistics_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/status_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/status_widget.cpp
)

if (LT_WINDOWS)
    list(APPEND LT_MODULE_VIDEO_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/decoder/openh264_decoder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/decoder/openh264_decoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/renderer/d3d11_pipeline.h
        ${CMAKE_CURRENT_SOURCE_DIR}/renderer/d3d11_pipeline.cpp
    )
elseif (LT_LINUX)
    list(APPEND LT_MODULE_VIDEO_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/renderer/va_gl_pipeline.h
        ${CMAKE_CURRENT_SOURCE_DIR}/renderer/va_gl_pipeline.cpp
    )
elseif (LT_MAC)
    list(APPEND LT_MODULE_VIDEO_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/renderer/vtb_gl_pipeline.h
        ${CMAKE_CURRENT_SOURCE_DIR}/renderer/vtb_gl_pipeline.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/renderer/vtb_gl_pipeline_plat.h
        ${CMAKE_CURRENT_SOURCE_DIR}/renderer/vtb_gl_pipeline_plat.m
    )
endif()

if (NOT LT_WINDOWS)
    list(REMOVE_ITEM LT_MODULE_VIDEO_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/capturer/dxgi_video_capturer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/capturer/dxgi_video_capturer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/capturer/dxgi/duplication_manager.h
        ${CMAKE_CURRENT_SOURCE_DIR}/capturer/dxgi/duplication_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/capturer/dxgi/common_types.h
        ${CMAKE_CURRENT_SOURCE_DIR}/cepipeline/video_capture_encode_pipeline.h
        ${CMAKE_CURRENT_SOURCE_DIR}/cepipeline/video_capture_encode_pipeline.cpp
    )
endif()

add_library(lt_module_video STATIC
    ${LT_MODULE_VIDEO_SRCS}
)

source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${LT_MODULE_VIDEO_SRCS})

target_include_directories(lt_module_video
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lt_module_video
    PRIVATE
        lt_build_config
        lt_module_ltlib
        lt_module_audio
        lt_module_inputs
        lt_module_plat
        g3log
        protobuf::libprotobuf-lite
        ltproto
        transport_api
        rtc
        SDL2::SDL2-static
        ffmpeg
        openh264
        amf
        imgui
        VPL::dispatcher
        nvcodec
)

if (LT_HAS_RTC2)
    target_link_libraries(lt_module_video PRIVATE rtc2)
endif()

if (LT_LINUX)
    target_link_libraries(lt_module_video
        PRIVATE
            PkgConfig::Va
            PkgConfig::Va-X11
            PkgConfig::Va-Wayland
            PkgConfig::Drm
            PkgConfig::Va-Drm
            PkgConfig::GL
            PkgConfig::EGL
            PkgConfig::GLib
            PkgConfig::MFX
    )
elseif (LT_MAC)
    target_link_libraries(lt_module_video
        PRIVATE
            OpenGL::GL
    )
endif()

set_code_analysis(lt_module_video ${LT_ENABLE_CODE_ANALYSIS})
