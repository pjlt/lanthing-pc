project(nvcodec)

if (NOT NVCODEC_VERSION)
    set(NVCODEC_VERSION 9)
endif()


add_library(${PROJECT_NAME} STATIC IMPORTED GLOBAL)

if (NVCODEC_VERSION EQUAL 9)
    set_target_properties(${PROJECT_NAME} PROPERTIES INTERFACE_LINK_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/v9.0.20/lib/win")
    set_target_properties(${PROJECT_NAME} PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/v9.0.20/lib/win/nvcuvid.lib")
    target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/v9.0.20/include")
elseif(NVCODEC_VERSION EQUAL 11)
    set_target_properties(${PROJECT_NAME} PROPERTIES INTERFACE_LINK_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/v11.0.10/lib/win")
    set_target_properties(${PROJECT_NAME} PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/v11.0.10/lib/win/nvcuvid.lib")
    set_target_properties(${PROJECT_NAME} PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/v11.0.10/lib/win/nvencodeapi.lib")
    target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/v11.0.10/include")
else()
    message(FATAL_ERROR "Unsupported NVCODEC_VERSION ${NVCODEC_VERSION}")
endif()