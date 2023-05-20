project(ltrtc)

add_library(${PROJECT_NAME} STATIC IMPORTED GLOBAL)
set_property(TARGET ${PROJECT_NAME} PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/win/ltrtc.lib")
target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
