cmake_minimum_required(VERSION 3.20)
project(lanthing)


set(LT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/string_keys.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/string_keys.cpp

    # Client
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/client.cpp
    # Client->graphics
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/graphics/ct_smoother.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/graphics/ct_smoother.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/graphics/d3d11_pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/graphics/d3d11_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/graphics/pixel_shader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/graphics/types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/graphics/vertex_shader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/graphics/video.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/graphics/video.cpp
    # Client->input
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/input/input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/input/input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/input/input_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/input/input_event.cpp
    # Client->platforms
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/platforms/pc_sdl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/platforms/pc_sdl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/platforms/pc_sdl_input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client/platforms/pc_sdl_input.cpp

    # Service
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/service.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/daemon/daemon.h
    # Service->settings
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/settings/settings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/settings/settings.cpp
    # Service->workers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_process.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_process.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_session.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/service/workers/worker_session.cpp

    # Worker
    ${CMAKE_CURRENT_SOURCE_DIR}/src/worker/message_handler.h
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
    # graphics->capturer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/video_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/video_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi_video_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi_video_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi/duplication_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi/duplication_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/capturer/dxgi/common_types.h
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

target_include_directories(${PROJECT_NAME}
    #让本项目的代码可以从"src"文件夹开始include
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

#服务器地址
target_compile_definitions(${PROJECT_NAME} PRIVATE LT_SERVER_ADDR=${LT_SERVER_ADDR})

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
    tomlpp
    SDL2::SDL2
    ffmpeg
    nvcodec
    mfx
    uv
    breakpad
    ltlib
    ltproto
    rtc
)

install(
    TARGETS ${PROJECT_NAME}
)