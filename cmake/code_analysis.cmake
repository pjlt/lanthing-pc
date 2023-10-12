function(set_code_analysis target)
if (LT_WINDOWS)
    target_compile_options(${target} PRIVATE /wd4819 /wd4251 /wd6326) #c4251这个警告跟dll导出有关，如果整个项目的编译平台、编译工具链、运行时都是一样的，理论上不会有问题
    target_compile_options(${target} PRIVATE /W4 /WX /external:W0 /external:I ${CMAKE_CURRENT_SOURCE_DIR}/third_party /analyze:rulesetdirectory ${LT_ROOT_DIR}/code_analysis/rulesets /analyze:external-)
else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
endfunction()