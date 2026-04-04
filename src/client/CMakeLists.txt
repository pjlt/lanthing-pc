set(LT_MODULE_CLIENT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/client.h
    ${CMAKE_CURRENT_SOURCE_DIR}/client.cpp
)

add_library(lt_module_client STATIC
    ${LT_MODULE_CLIENT_SRCS}
)

source_group(TREE ${CMAKE_SOURCE_DIR}/src FILES ${LT_MODULE_CLIENT_SRCS})

target_include_directories(lt_module_client
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lt_module_client
    PRIVATE
        lt_module_ltlib
        g3log
        protobuf::libprotobuf-lite
        ltproto
        rtc
        transport_api
)

if(LT_HAS_RTC2)
    target_link_libraries(lt_module_client PRIVATE rtc2)
endif()

set_code_analysis(lt_module_client ${LT_ENABLE_CODE_ANALYSIS})
