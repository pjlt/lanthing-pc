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
elseif(LT_LINUX)
    enable_language(ASM)
    set(BREAKPAD_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/../breakpad/src)
    set(BREAKPAD_INC_FOLDER ${BREAKPAD_SRC_FOLDER}/client/linux/handler)
    set(BREAKPAD_SOURCES
        ${BREAKPAD_SRC_FOLDER}/client/linux/crash_generation/crash_generation_client.cc
        ${BREAKPAD_SRC_FOLDER}/client/linux/dump_writer_common/thread_info.cc
        ${BREAKPAD_SRC_FOLDER}/client/linux/dump_writer_common/ucontext_reader.cc
        ${BREAKPAD_SRC_FOLDER}/client/linux/handler/exception_handler.cc
        ${BREAKPAD_SRC_FOLDER}/client/linux/handler/minidump_descriptor.cc
        ${BREAKPAD_SRC_FOLDER}/client/linux/log/log.cc
        ${BREAKPAD_SRC_FOLDER}/client/linux/microdump_writer/microdump_writer.cc
        ${BREAKPAD_SRC_FOLDER}/client/linux/minidump_writer/linux_core_dumper.cc
        ${BREAKPAD_SRC_FOLDER}/client/linux/minidump_writer/linux_dumper.cc
        ${BREAKPAD_SRC_FOLDER}/client/linux/minidump_writer/linux_ptrace_dumper.cc
        ${BREAKPAD_SRC_FOLDER}/client/linux/minidump_writer/minidump_writer.cc
        ${BREAKPAD_SRC_FOLDER}/client/minidump_file_writer.cc

        ${BREAKPAD_SRC_FOLDER}/common/convert_UTF.cc
        ${BREAKPAD_SRC_FOLDER}/common/md5.cc
        ${BREAKPAD_SRC_FOLDER}/common/string_conversion.cc
        ${BREAKPAD_SRC_FOLDER}/common/linux/breakpad_getcontext.S
        ${BREAKPAD_SRC_FOLDER}/common/linux/http_upload.cc
        ${BREAKPAD_SRC_FOLDER}/common/linux/elfutils.cc
        ${BREAKPAD_SRC_FOLDER}/common/linux/elf_core_dump.cc
        ${BREAKPAD_SRC_FOLDER}/common/linux/file_id.cc
        ${BREAKPAD_SRC_FOLDER}/common/linux/guid_creator.cc
        ${BREAKPAD_SRC_FOLDER}/common/linux/linux_libc_support.cc
        ${BREAKPAD_SRC_FOLDER}/common/linux/memory_mapped_file.cc
        ${BREAKPAD_SRC_FOLDER}/common/linux/safe_readlink.cc
    )
elseif(LT_MAC)
    set(BREAKPAD_SRC_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/../breakpad/src)
    set(BREAKPAD_INC_FOLDER
        ${BREAKPAD_SRC_FOLDER}/client/mac/handler
        ${BREAKPAD_SRC_FOLDER}/client/apple/Framework
    )
    set(BREAKPAD_SOURCES
        ${BREAKPAD_SRC_FOLDER}/client/mac/Framework/Breakpad.mm
        ${BREAKPAD_SRC_FOLDER}/client/mac/Framework/OnDemandServer.mm
        ${BREAKPAD_SRC_FOLDER}/client/mac/crash_generation/crash_generation_client.cc
        ${BREAKPAD_SRC_FOLDER}/client/mac/crash_generation/crash_generation_client.h
        ${BREAKPAD_SRC_FOLDER}/client/mac/handler/protected_memory_allocator.cc
        ${BREAKPAD_SRC_FOLDER}/client/mac/handler/exception_handler.cc  
        ${BREAKPAD_SRC_FOLDER}/client/mac/handler/minidump_generator.cc  
        ${BREAKPAD_SRC_FOLDER}/client/mac/handler/dynamic_images.cc  
        ${BREAKPAD_SRC_FOLDER}/client/mac/handler/breakpad_nlist_64.cc  
        ${BREAKPAD_SRC_FOLDER}/client/minidump_file_writer.cc  
        ${BREAKPAD_SRC_FOLDER}/common/mac/MachIPC.mm 
        ${BREAKPAD_SRC_FOLDER}/common/mac/string_utilities.cc
        ${BREAKPAD_SRC_FOLDER}/common/mac/file_id.cc
        ${BREAKPAD_SRC_FOLDER}/common/mac/macho_id.cc
        ${BREAKPAD_SRC_FOLDER}/common/mac/macho_utilities.cc
        ${BREAKPAD_SRC_FOLDER}/common/mac/macho_walker.cc
        ${BREAKPAD_SRC_FOLDER}/common/mac/HTTPMultipartUpload.m
        ${BREAKPAD_SRC_FOLDER}/common/mac/HTTPRequest.m
        ${BREAKPAD_SRC_FOLDER}/common/mac/bootstrap_compat.cc
        ${BREAKPAD_SRC_FOLDER}/common/string_conversion.cc
        ${BREAKPAD_SRC_FOLDER}/common/convert_UTF.cc
        ${BREAKPAD_SRC_FOLDER}/common/md5.cc
        ${BREAKPAD_SRC_FOLDER}/common/simple_string_dictionary.cc
        ${BREAKPAD_SRC_FOLDER}/common/long_string_dictionary.cc
    )
endif()

add_library(${PROJECT_NAME}
    ${BREAKPAD_SOURCES}
    ${API_SOURCES}
)



target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${BREAKPAD_SRC_FOLDER}
        ${BREAKPAD_INC_FOLDER}
        ${CMAKE_SOURCE_DIR}
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        UNICODE
        BUILDING_MINIDUMP=1
)

if (LT_WINDOWS)
    target_link_libraries(${PROJECT_NAME} wininet)
else()
    target_link_libraries(${PROJECT_NAME} dl pthread)
endif(LT_WINDOWS)

install(
    TARGETS ${PROJECT_NAME}
)
