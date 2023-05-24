cmake_minimum_required(VERSION 3.20)
project(lanthing-qt)

add_executable(${PROJECT_NAME}
	${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/client_ui.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/client_ui.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/client_session.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/client_session.cpp
)

target_link_libraries(${PROJECT_NAME}
	g3log
	uv_a
	protobuf::libprotobuf-lite
	ltproto
	ltlib
	ltrtc
)