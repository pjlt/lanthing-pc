cmake_minimum_required(VERSION 3.21)

project(rtc2)

set(CMAKE_CXX_STANDARD 20)

set(RTC2_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/include/rtc2/rtc2.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rtc2/exports.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rtc2/connection.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rtc2/key_and_cert.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rtc2/video_frame.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rtc2/codec_types.h

	${CMAKE_CURRENT_SOURCE_DIR}/src/rtc2.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/src/connection/connection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/connection/connection_impl.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/connection/connection_impl.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/src/stream/audio_receive_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/stream/audio_receive_stream.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/stream/audio_send_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/stream/audio_send_stream.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/stream/video_receive_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/stream/video_receive_stream.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/stream/video_send_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/stream/video_send_stream.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/stream/message_channel.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/stream/message_channel.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/buffer.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/buffer.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/cc/bwe.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/cc/bwe.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/cc/pacer.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/cc/pacer.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/rtp/rtp_extention.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/rtp/rtp_extention.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/rtp/rtp_packet.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/rtp/rtp_packet.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/video/frame_assembler.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/video/frame_assembler.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/message/reliable_message_channel.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/message/reliable_message_channel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/message/half_reliable_message_channel.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/message/half_reliable_message_channel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/message/ikcp.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/message/ikcp.c

	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/dtls/dtls_channel.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/dtls/dtls_channel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/dtls/mbed_dtls.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/dtls/mbed_dtls.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/dtls/queue.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/dtls/key_and_cert.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/p2p.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/p2p.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/netcard.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/netcard.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/endpoint.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/endpoint.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/lan_endpoint.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/lan_endpoint.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/wan_endpoint.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/wan_endpoint.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/relay_endpoint.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/relay_endpoint.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/attributes_template.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/crc32.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/crc32.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/easy_stun.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/easy_stun.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/hmac_sha1.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/hmac_sha1.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/md5.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/md5.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/sha1.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/sha1.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/message.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/msg.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/p2p/stuns/stun_msg.c

	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/network/address.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/network/address.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/network/network_channel.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/network/network_channel.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/network/udp_socket.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/modules/network/udp_socket.cpp
)

add_library(${PROJECT_NAME} STATIC
	${RTC2_SRCS}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${RTC2_SRCS})

target_include_directories(${PROJECT_NAME}
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${PROJECT_NAME}
	PRIVATE
		BUILDING_RTC2=1
)

set_code_analysis(${PROJECT_NAME} ${LT_ENABLE_CODE_ANALYSIS})

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		transport_api
		g3log
		uv_a
		MbedTLS::mbedtls
		MbedTLS::mbedx509
		MbedTLS::mbedcrypto
		ltlib
)

install(
	TARGETS ${PROJECT_NAME}
)