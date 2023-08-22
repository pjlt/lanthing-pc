cmake_minimum_required(VERSION 3.20)
project(transport)

add_library(${PROJECT_NAME}
	${CMAKE_CURRENT_SOURCE_DIR}/include/transport/transport.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/transport/transport_tcp.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/transport_tcp.cpp
)

target_include_directories(${PROJECT_NAME}
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		protobuf::libprotobuf-lite
		g3log
		ltlib
		ltproto
)