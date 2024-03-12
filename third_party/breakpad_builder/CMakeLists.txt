cmake_minimum_required(VERSION 3.20)
project(breakpad)


set(API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/lt_minidump_generator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lt_minidump_generator.cpp
)

if (LT_WINDOWS)
    set(BREAKPAD_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/../breakpad/src)
    set(BREAKPAD_INC_FOLDER ${BREAKPAD_SRC_FOLDER}/client/windows/handler)
    set(BREAKPAD_SOURCES
        ${BREAKPAD_SRC_FOLDER}/client/windows/crash_generation/crash_generation_client.cc
        ${BREAKPAD_SRC_FOLDER}/client/windows/handler/exception_handler.cc
        ${BREAKPAD_SRC_FOLDER}/common/windows/guid_string.cc
        ${BREAKPAD_SRC_FOLDER}/common/windows/string_utils.cc
        ${BREAKPAD_SRC_FOLDER}/common/windows/http_upload.cc
    )
    # 按照原始目录结构展开
    source_group(breakpad TREE ${BREAKPAD_SRC_FOLDER} FILES ${BREAKPAD_SOURCES})
    source_group(breakpad_builder TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${API_SOURCES})
else()
    set(BREAKPAD_SOURCES)
    set(BREAKPAD_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/../breakpad/src)
    set(BREAKPAD_INC_FOLDER ${BREAKPAD_SRC_FOLDER}/client/linux/handler)
endif()

add_library(${PROJECT_NAME}
    ${BREAKPAD_SOURCES}
    ${API_SOURCES}
)



target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${BREAKPAD_SRC_FOLDER}
        ${BREAKPAD_INC_FOLDER}
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        UNICODE
        BUILDING_MINIDUMP=1
)

target_link_libraries(${PROJECT_NAME} wininet)

install(
    TARGETS ${PROJECT_NAME}
)
