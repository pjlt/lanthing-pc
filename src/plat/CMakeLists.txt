set(LT_MODULE_PLAT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/pc_sdl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pc_sdl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pc_sdl_input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/pc_sdl_input.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/video_device.h
)

if (LT_WINDOWS)
    list(APPEND LT_MODULE_PLAT_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/video_device_win.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/d3d11_video_device.h
        ${CMAKE_CURRENT_SOURCE_DIR}/d3d11_video_device.cpp
    )
else()
    list(APPEND LT_MODULE_PLAT_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/video_device_stub.cpp
    )
endif()

add_library(lt_module_plat STATIC
    ${LT_MODULE_PLAT_SRCS}
)

source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${LT_MODULE_PLAT_SRCS})

target_include_directories(lt_module_plat
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lt_module_plat
    PRIVATE
        lt_build_config
        lt_module_inputs
        SDL2::SDL2-static
    g3log
    transport_api
    ltproto
    protobuf::libprotobuf-lite
)

set_code_analysis(lt_module_plat ${LT_ENABLE_CODE_ANALYSIS})
