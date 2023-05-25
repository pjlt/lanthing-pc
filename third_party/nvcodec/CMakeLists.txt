project(nvcodec)

add_library(${PROJECT_NAME}
	${CMAKE_CURRENT_SOURCE_DIR}/include/nvcodec/nvEncodeAPI.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/nvcodec/NvEncoder.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/nvcodec/NvEncoderCLIOptions.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/nvcodec/NvEncoderD3D9.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/nvcodec/NvEncoderD3D11.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/NvEncoder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/NvEncoderD3D9.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/NvEncoderD3D11.cpp
)


target_include_directories(${PROJECT_NAME}
	PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
)
