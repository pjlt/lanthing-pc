
function(deploy_qt6 target) 
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND windeployqt.exe 
            $<TARGET_FILE_DIR:${target}>
    )
endfunction()
