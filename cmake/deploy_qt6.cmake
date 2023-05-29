
function(deploy_qt6 target deployqt_executable) 
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${deployqt_executable}
            $<TARGET_FILE_DIR:${target}>
    )
endfunction()
