install(
    TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION .
    ARCHIVE DESTINATION .
)

if (LT_WINDOWS)
    install(CODE "execute_process(COMMAND ${WINDEPLOYQT_EXECUTABLE} --no-translations --no-system-d3d-compiler --no-quick-import --pdb ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${PROJECT_NAME}.exe)")
    install(
        FILES $<TARGET_PDB_FILE:${PROJECT_NAME}>
        DESTINATION ${CMAKE_INSTALL_PREFIX}/pdb
    )
endif()


if (LT_WINDOWS)
install(CODE [[
    file(GET_RUNTIME_DEPENDENCIES
        RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPS
        UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPS
        EXECUTABLES $<TARGET_FILE:lanthing>
        DIRECTORIES ${CMAKE_SOURCE_DIR}
        PRE_INCLUDE_REGEXES ${CMAKE_SOURCE_DIR}
        POST_INCLUDE_REGEXES ${CMAKE_SOURCE_DIR}
        PRE_EXCLUDE_REGEXES "system32"
        POST_EXCLUDE_REGEXES "system32"
    )
    foreach(DEP_LIB ${RESOLVED_DEPS})
        file(INSTALL ${DEP_LIB} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
    endforeach()
]])
endif()

if (LT_WINDOWS)
deploy_dlls(${PROJECT_NAME})
deploy_qt6(${PROJECT_NAME} ${WINDEPLOYQT_EXECUTABLE})
endif(LT_WINDOWS)