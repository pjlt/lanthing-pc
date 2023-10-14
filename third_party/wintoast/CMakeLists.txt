project(wintoastlib VERSION 1.3.0)

add_library(${PROJECT_NAME} STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/wintoastlib.h
	${CMAKE_CURRENT_SOURCE_DIR}/wintoastlib.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})