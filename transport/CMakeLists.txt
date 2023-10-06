cmake_minimum_required(VERSION 3.20)
project(transport)

add_library(${PROJECT_NAME}_api INTERFACE)
target_include_directories(${PROJECT_NAME}_api INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${PROJECT_NAME}
	${CMAKE_CURRENT_SOURCE_DIR}/include/transport/transport.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/transport/transport_tcp.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/transport/transport_rtc.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/transport/transport_rtc2.h
	${CMAKE_CURRENT_SOURCE_DIR}/tcp/transport_tcp.cpp
)

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		${PROJECT_NAME}_api
	PRIVATE
		protobuf::libprotobuf-lite
		g3log
		ltlib
		ltproto
)

add_subdirectory(rtc)
add_subdirectory(rtc2)

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		rtc
		rtc2
)