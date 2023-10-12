function(set_code_analysis target)
if (LT_WINDOWS)
    target_compile_options(${target} PRIVATE /wd4819 /wd4251 /wd6326) #c4251��������dll�����йأ����������Ŀ�ı���ƽ̨�����빤����������ʱ����һ���ģ������ϲ���������
    target_compile_options(${target} PRIVATE /W4 /WX /external:W0 /external:I ${CMAKE_CURRENT_SOURCE_DIR}/third_party /analyze:rulesetdirectory ${LT_ROOT_DIR}/code_analysis/rulesets /analyze:external-)
else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
endfunction()