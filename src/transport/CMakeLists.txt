cmake_minimum_required(VERSION 3.21)
project(transport)

add_library(${PROJECT_NAME}_api INTERFACE)
target_include_directories(${PROJECT_NAME}_api INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/include/transport/transport.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/transport/transport_tcp.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/transport/transport_rtc.h
	${CMAKE_CURRENT_SOURCE_DIR}/tcp/transport_tcp.cpp
)

if(LT_HAS_RTC2)
	list(APPEND SRCS ${CMAKE_CURRENT_SOURCE_DIR}/include/transport/transport_rtc2.h)
endif(LT_HAS_RTC2)

add_library(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		${PROJECT_NAME}_api
	PRIVATE
		protobuf::libprotobuf-lite
		g3log
		uv_a
		ltproto
)

set(DEP_LIBS rtc)

add_subdirectory(rtc/${LT_PLAT})
if(LT_HAS_RTC2)
	add_subdirectory(rtc2)
	list(APPEND DEP_LIBS rtc2)
endif(LT_HAS_RTC2)
#

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		${DEP_LIBS}
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_SOURCE_DIR}/src
)