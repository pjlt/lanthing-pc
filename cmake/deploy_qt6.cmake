
function(prepare_qt6)
    if(NOT DEFINED LT_QT_CMAKE_PATH)
        message(FATAL_ERROR "LT_QT_CMAKE_PATH not set!")
    endif()
    list(APPEND CMAKE_PREFIX_PATH ${LT_QT_CMAKE_PATH})
    qt_standard_project_setup()
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")
endfunction()

function(deploy_qt6 target) 
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND windeployqt.exe 
            $<TARGET_FILE_DIR:${target}>
    )
endfunction()
