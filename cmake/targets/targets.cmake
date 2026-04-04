set(LT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lt_constants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/firewall.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/firewall.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/message_handler.h
)

if (LT_WINDOWS)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/lanthing.rc.in
        ${CMAKE_CURRENT_SOURCE_DIR}/lanthing.rc
        @ONLY)

    set(LT_LANTHING_RC ${CMAKE_CURRENT_SOURCE_DIR}/lanthing.rc)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/ltlib)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/audio)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/inputs)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/plat)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/client)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/service)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/worker)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/video)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/app)

add_executable(${PROJECT_NAME}
    ${LT_LANTHING_RC}
    ${LT_SRCS}
)

# 按照原始目录结构展开
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${LT_SRCS} ${PLATFORM_SRCS})

qt_add_translations(${PROJECT_NAME}
    TS_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/i18n/lt-zh_CN.ts
)

target_include_directories(${PROJECT_NAME}
    #让本项目的代码可以从"src"文件夹开始include
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE -DBUILDING_LT_EXE=1)
qt_disable_unicode_defines(${PROJECT_NAME})

set_code_analysis(${PROJECT_NAME} ${LT_ENABLE_CODE_ANALYSIS})

if(LT_WINDOWS)
    if(LT_USE_PREBUILT_VIDEO2)
        #add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/prebuilt/video2/${LT_THIRD_POSTFIX})
    else(LT_USE_PREBUILT_VIDEO2)
        #add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/video2)
    endif(LT_USE_PREBUILT_VIDEO2)
    set(PLATFORM_LIBS
            #video2
            openh264
            Secur32.lib dmoguids.lib d3d11.lib d3dcompiler.lib
            dxgi.lib Msdmo.lib Dxva2.lib winmm.lib wmcodecdspuuid.lib
            Dwmapi.lib Mfplat.lib Bcrypt.lib Mfuuid.lib Strmiids.lib
            ViGEmClient::ViGEmClientShared
            VPL::dispatcher nvcodec wintoastlib nbclipboard
    )
elseif(LT_LINUX)
    set(PLATFORM_LIBS
        m
        stdc++
        PkgConfig::X11
        PkgConfig::Va
        PkgConfig::Va-X11
        PkgConfig::Va-Wayland
        PkgConfig::Drm
        PkgConfig::Va-Drm
        PkgConfig::GL
        PkgConfig::EGL
        PkgConfig::GLib
        PkgConfig::MFX
        VPL::dispatcher
    )
elseif(LT_MAC)
    set(PLATFORM_LIBS
        OpenGL::GL
    )
    set(PLATFORM_LIBS "${PLATFORM_LIBS}" -lobjc ${LIB_CORE_VIDEO} ${LIB_APP_KIT} ${LIB_IO_SURFACE})
else()
    set(PLATFORM_LIBS)
endif()

set(TRANSPORT_LIBS rtc transport_api transport)
if(LT_HAS_RTC2)
    list(APPEND TRANSPORT_LIBS rtc2)
endif()

target_link_libraries(${PROJECT_NAME}
    lt_module_ltlib
    lt_module_audio
    lt_module_inputs
    lt_module_plat
    lt_module_client
    lt_module_service
    lt_module_worker
    lt_module_video
    lt_module_app
    Qt6::Widgets
    Qt6::Gui
    g3log
    protobuf::libprotobuf-lite
    SDL2::SDL2-static
    ffmpeg
    Opus::opus
    amf
    uv_a
    breakpad
    imgui
    utf8cpp
    sqlite3
    tomlpp
    MbedTLS::mbedtls
    MbedTLS::mbedcrypto
    MbedTLS::mbedx509
    ltproto
    ${TRANSPORT_LIBS}
    #fonts
    ${PLATFORM_LIBS}
)

if (LT_WINDOWS)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
endif(LT_WINDOWS)

# 设置VS调试路径
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>")