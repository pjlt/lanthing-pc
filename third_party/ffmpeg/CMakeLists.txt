project(ffmpeg)

add_library(${PROJECT_NAME}::avcodec UNKNOWN IMPORTED GLOBAL)
add_library(${PROJECT_NAME}::avutil UNKNOWN IMPORTED GLOBAL)
add_library(${PROJECT_NAME}::swresample UNKNOWN IMPORTED GLOBAL)

set_property(TARGET ${PROJECT_NAME}::avcodec PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/libavcodec.a")
set_property(TARGET ${PROJECT_NAME}::avutil PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/libavutil.a")
set_property(TARGET ${PROJECT_NAME}::swresample PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/libswresample.a")

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_property(TARGET ${PROJECT_NAME} PROPERTY INTERFACE_LINK_LIBRARIES
	${PROJECT_NAME}::avcodec
	${PROJECT_NAME}::avutil
	${PROJECT_NAME}::swresample
)
