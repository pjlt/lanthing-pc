cmake_minimum_required(VERSION 3.20)
project(imgui)

set(IMGUI_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/../imgui)

if(LT_WINDOWS)
set(PLATFORM_SOURCES
     ${IMGUI_FOLDER}/backends/imgui_impl_dx11.h
     ${IMGUI_FOLDER}/backends/imgui_impl_dx11.cpp
     ${IMGUI_FOLDER}/backends/imgui_impl_sdl2.h
     ${IMGUI_FOLDER}/backends/imgui_impl_sdl2.cpp
     ${IMGUI_FOLDER}/backends/imgui_impl_win32.h
     ${IMGUI_FOLDER}/backends/imgui_impl_win32.cpp
)
else()
set(PLATFORM_SOURCES)
endif()

set(IMGUI_SOURCES
    ${IMGUI_FOLDER}/imgui.h
    ${IMGUI_FOLDER}/imgui.cpp
    ${IMGUI_FOLDER}/imgui_draw.cpp
    ${IMGUI_FOLDER}/imgui_internal.h
    ${IMGUI_FOLDER}/imgui_tables.cpp
    ${IMGUI_FOLDER}/imgui_widgets.cpp
    ${PLATFORM_SOURCES}
)

add_library(${PROJECT_NAME}
    ${IMGUI_SOURCES}
)

# 按照原始目录结构展开
source_group(imgui TREE ${IMGUI_FOLDER} FILES ${IMGUI_SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${IMGUI_FOLDER}
)


target_link_libraries(${PROJECT_NAME}
    SDL2::SDL2
)