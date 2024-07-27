if (LT_LINUX)
    set(PLAT_LIBS m)
else()
    set(PLAT_LIBS)
endif()

add_executable(test_settings
    ${CMAKE_CURRENT_SOURCE_DIR}/src/settings_tests.cpp
)
target_link_libraries(test_settings
    g3log
    sqlite3
    GTest::gtest
    GTest::gtest_main
    #${PROJECT_NAME} # TODO: ªÿ∏¥÷ß≥÷≤‚ ‘
    ${PLAT_LIBS}
)
add_test(NAME test_settings COMMAND test_settings)