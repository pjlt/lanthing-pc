cmake_minimum_required(VERSION 3.20)
project(ltproto)

option(GENERATE_LTPROTO OFF)

#option("PROTOC_PATH" "The path of protoc" "${CMAKE_CURRENT_SOURCE_DIR}/protoc.exe")
set(PROTOC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/protoc.exe)


function(lt_generate_protobuf_cpp SRCS HDRS)
    set(_SRCS)
    set(_HDRS)
    foreach (_file ${ARGN})
        string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} _file1 ${_file})
        string(REPLACE ".proto" ".pb.cc" cc_file ${_file1})
        string(REPLACE ".proto" ".pb.h" h_file ${_file1})
        list(APPEND _SRCS ${cc_file})
        list(APPEND _HDRS ${h_file})
    endforeach()
    add_custom_command(
      OUTPUT ${_SRCS} ${_HDRS}
      COMMAND ${PROTOC_PATH}
      ARGS --cpp_out=lite:${CMAKE_CURRENT_BINARY_DIR} --proto_path=${CMAKE_CURRENT_SOURCE_DIR} ${ARGN}
      DEPENDS ${ARGN}
    )
    set(${SRCS} ${_SRCS} PARENT_SCOPE)
    set(${HDRS} ${_HDRS} PARENT_SCOPE)
endfunction()

set(LT_PROTOS
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/capture_video_frame.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/controller_added_removed.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/controller_response.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/controller_status.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/keep_alive.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/keyboard_event.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/mouse_click.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/mouse_motion.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/mouse_wheel.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/start_transmission.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/start_transmission_ack.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/start_working.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/start_working_ack.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/stop_transmission.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/stop_working.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/streaming_params.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/switch_hid_driver.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/video_codec_type.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/video_frame.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/video_frame_ack1.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/peer2peer/video_frame_ack2.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/allocate_device_id.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/allocate_device_id_ack.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/close_connection.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/connection_type.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/login_device.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/login_device_ack.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/login_user.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/login_user_ack.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/open_connection.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/open_connection_ack.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/request_connection.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/server/request_connection_ack.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/signaling/join_room.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/signaling/join_room_ack.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/signaling/signaling_message.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/signaling/signaling_message_ack.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/ltproto/ui/push_device_id.proto
)

lt_generate_protobuf_cpp(GENERATED_PROTO_CC GENERATED_PROTO_H ${LT_PROTOS})

if(GENERATE_LTPROTO)
    add_custom_command(
        OUTPUT
            ${CMAKE_CURRENT_SOURCE_DIR}/include/ltproto/ltproto.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ltproto.cpp
        COMMAND
            python ${CMAKE_CURRENT_SOURCE_DIR}/gencpp.py
    )
endif()

add_library(${PROJECT_NAME} STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ltproto/ltproto.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltproto.cpp
    ${GENERATED_PROTO_CC}
    ${GENERATED_PROTO_H}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        protobuf::libprotobuf-lite
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
