function(_lt_qt_deploy_args out_var)
    # Keep one shared argument list for dev/post-build and install-time deployment.
    set(_args
        --no-translations
        --no-system-d3d-compiler
        --no-quick-import
        --pdb
    )
    set(${out_var} "${_args}" PARENT_SCOPE)
endfunction()

function(deploy_qt6_post_build target deployqt_executable)
    if (NOT deployqt_executable)
        return()
    endif()

    _lt_qt_deploy_args(_qt_deploy_args)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${deployqt_executable} ${_qt_deploy_args} $<TARGET_FILE_DIR:${target}>
        COMMAND_EXPAND_LISTS
    )
endfunction()

function(install_qt6_runtime target deployqt_executable)
    if (NOT deployqt_executable)
        return()
    endif()

    _lt_qt_deploy_args(_qt_deploy_args)
    string(REPLACE ";" " " _qt_deploy_args_string "${_qt_deploy_args}")

    install(CODE
        "execute_process(COMMAND \"${deployqt_executable}\" ${_qt_deploy_args_string} \"\${CMAKE_INSTALL_PREFIX}/bin/${target}.exe\")"
    )
endfunction()
