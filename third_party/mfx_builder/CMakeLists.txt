cmake_minimum_required( VERSION 3.6 FATAL_ERROR )
project(mfx)

set(MFX_API_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/../IntelMediaSDK/api")
set(MFX_SRC_FOLDER "${MFX_API_FOLDER}/mfx_dispatch/windows/src")
set(MFX_INTERNAL_INCLUDE_FOLDER "${MFX_API_FOLDER}/mfx_dispatch/windows/include")
set(MFX_SOURCES
    ${MFX_SRC_FOLDER}/main.cpp
    ${MFX_SRC_FOLDER}/mfx_critical_section.cpp
    ${MFX_SRC_FOLDER}/mfx_dispatcher.cpp
    ${MFX_SRC_FOLDER}/mfx_dispatcher_log.cpp
    ${MFX_SRC_FOLDER}/mfx_driver_store_loader.cpp
    ${MFX_SRC_FOLDER}/mfx_dxva2_device.cpp
    ${MFX_SRC_FOLDER}/mfx_function_table.cpp
    ${MFX_SRC_FOLDER}/mfx_library_iterator.cpp
    ${MFX_SRC_FOLDER}/mfx_load_dll.cpp
    ${MFX_SRC_FOLDER}/mfx_load_plugin.cpp
    ${MFX_SRC_FOLDER}/mfx_plugin_hive.cpp
    ${MFX_SRC_FOLDER}/mfx_win_reg_key.cpp
)

add_library(${PROJECT_NAME} STATIC
    ${MFX_SOURCES}
)

# 按照原始目录结构展开
source_group(TREE ${MFX_SRC_FOLDER} PREFIX src FILES ${MFX_SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        "${MFX_API_FOLDER}/include"
    PRIVATE
        "${MFX_INTERNAL_INCLUDE_FOLDER}"
)
