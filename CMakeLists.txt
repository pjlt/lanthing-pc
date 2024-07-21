cmake_minimum_required(VERSION 3.21)

project(lanthing)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/options-user.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/options-user.cmake)
else()
    include(${CMAKE_CURRENT_SOURCE_DIR}/options-default.cmake)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeRC.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/code_analysis.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_dlls.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_qt6.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/definitions/definitions.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies/dependencies.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/targets/targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/postbuild/postbuild.cmake)

