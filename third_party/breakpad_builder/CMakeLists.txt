cmake_minimum_required(VERSION 3.20)
project(breakpad)

set(BREAKPAD_SRC_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/../breakpad/src")
set(BREAKPAD_INC_FOLDER "${BREAKPAD_SRC_FOLDER}/client/windows/handler")
set(API_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/lt_minidump_generator.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/lt_minidump_generator.cpp"
)
set(BREAKPAD_SOURCES
	"${BREAKPAD_SRC_FOLDER}/client/windows/crash_generation/crash_generation_client.cc"
	"${BREAKPAD_SRC_FOLDER}/client/windows/handler/exception_handler.cc"
	"${BREAKPAD_SRC_FOLDER}/common/windows/guid_string.cc"
	"${BREAKPAD_SRC_FOLDER}/common/windows/string_utils.cc"
	${API_SOURCES}
)

add_library(${PROJECT_NAME} SHARED
	${BREAKPAD_SOURCES}
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
		${BREAKPAD_SRC_FOLDER}
		${BREAKPAD_INC_FOLDER}
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${PROJECT_NAME}
	PRIVATE
		UNICODE
		BUILDING_MINIDUMP=1
)

install(
	TARGETS ${PROJECT_NAME}
)