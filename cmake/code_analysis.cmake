function(set_code_analysis target enable)
if (LT_WINDOWS)
    #c4251这个警告跟dll导出有关，如果整个项目的编译平台、编译工具链、运行时都是一样的，理论上不会有问题
    if (${enable})
        message(STATUS "${target} enable code analysis")
        target_compile_options(${target} PRIVATE /W4 /WX /wd4819 /wd4251 /wd6326 /wd4702 /external:W0 /external:I ${CMAKE_CURRENT_SOURCE_DIR}/third_party /analyze:external-)
    endif()
else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic -Werror -Wno-unknown-warning-option -Wno-unused-private-field -Wno-newline-eof -Wno-unknown-pragmas)
endif()
endfunction()
