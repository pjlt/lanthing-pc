option(LT_ENABLE_POST_BUILD_DEPLOYMENT "Deploy runtime dependencies after build for local runs" ON)
option(LT_ENABLE_INSTALL_RUNTIME_DEPLOYMENT "Deploy runtime dependencies during install/package" ON)

if (LT_WINDOWS AND LT_ENABLE_INSTALL_RUNTIME_DEPLOYMENT)
    install_target_with_runtime_deps(${PROJECT_NAME})
else()
    install(
        TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION .
    )
endif()

if (LT_WINDOWS)
    if (LT_ENABLE_INSTALL_RUNTIME_DEPLOYMENT)
        install_qt6_runtime(${PROJECT_NAME} ${WINDEPLOYQT_EXECUTABLE})
    endif()

    install(
        FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
        DESTINATION ${CMAKE_INSTALL_PREFIX}/pdb
    )

    if (LT_ENABLE_POST_BUILD_DEPLOYMENT)
        deploy_runtime_dlls_post_build(${PROJECT_NAME})
        deploy_qt6_post_build(${PROJECT_NAME} ${WINDEPLOYQT_EXECUTABLE})
    endif()

    include(InstallRequiredSystemLibraries)
endif()