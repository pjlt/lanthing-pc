project(lodepng)

add_library(${PROJECT_NAME}
	${CMAKE_CURRENT_SOURCE_DIR}/lodepng.h
	${CMAKE_CURRENT_SOURCE_DIR}/lodepng.cpp
)

target_include_directories(${PROJECT_NAME}
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(${PROJECT_NAME}
	PRIVATE
		/wd4267 /wd4334
)