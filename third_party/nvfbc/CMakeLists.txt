project(nvfbc)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME}
	INTERFACE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
)