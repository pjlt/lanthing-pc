set(LT_MODULE_AUDIO_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/audio_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/audio_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/fake_audio_capturer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/capturer/fake_audio_capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/player/audio_player.h
    ${CMAKE_CURRENT_SOURCE_DIR}/player/audio_player.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/player/sdl_audio_player.h
    ${CMAKE_CURRENT_SOURCE_DIR}/player/sdl_audio_player.cpp
)

if (LT_WINDOWS)
    list(APPEND LT_MODULE_AUDIO_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/capturer/win_audio_capturer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/capturer/win_audio_capturer.cpp
    )
endif()

add_library(lt_module_audio STATIC
    ${LT_MODULE_AUDIO_SRCS}
)

source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${LT_MODULE_AUDIO_SRCS})

target_include_directories(lt_module_audio
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lt_module_audio
    PRIVATE
        g3log
        protobuf::libprotobuf-lite
        ltproto
        transport_api
    SDL2::SDL2-static
    ffmpeg
    Opus::opus
)

set_code_analysis(lt_module_audio ${LT_ENABLE_CODE_ANALYSIS})
