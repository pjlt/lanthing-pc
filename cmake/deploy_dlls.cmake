function(deploy_runtime_dlls_post_build target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${target}>
        $<TARGET_FILE_DIR:${target}>
        COMMAND_EXPAND_LISTS
        )
endfunction()

function(install_target_with_runtime_deps target)
    install(
        TARGETS ${target}
        RUNTIME_DEPENDENCY_SET ${target}_runtime_deps
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION .
    )

    if (LT_WINDOWS)
        install(
            RUNTIME_DEPENDENCY_SET ${target}_runtime_deps
            DESTINATION bin
            DIRECTORIES ${CMAKE_SOURCE_DIR}
            PRE_INCLUDE_REGEXES ${CMAKE_SOURCE_DIR}
            POST_INCLUDE_REGEXES ${CMAKE_SOURCE_DIR}
            PRE_EXCLUDE_REGEXES
                "system32"
                "^api-ms-win-.*"
                "^ext-ms-win-.*"
            POST_EXCLUDE_REGEXES
                "system32"
        )
    endif()
endfunction()
