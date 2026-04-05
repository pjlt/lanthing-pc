if (NOT LT_WINDOWS)
    return()
endif()

set(LT_MODULE_CLIPBOARD_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/nbclipboard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/nbclipboard.cpp
)

add_library(lt_module_clipboard STATIC
    ${LT_MODULE_CLIPBOARD_SRCS}
)

source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${LT_MODULE_CLIPBOARD_SRCS})

target_include_directories(lt_module_clipboard
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lt_module_clipboard
    PRIVATE
        lt_build_config
)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/nbclipboard.cpp PROPERTIES
    COMPILE_DEFINITIONS "UNICODE;_UNICODE"
)

set_code_analysis(lt_module_clipboard ${LT_ENABLE_CODE_ANALYSIS})
