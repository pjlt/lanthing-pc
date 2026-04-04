include_guard(GLOBAL)

# Keep CTest as the unified entry for all module-level tests.
include(CTest)
if (BUILD_TESTING)
    enable_testing()
    message(STATUS "CTest integration enabled (LT_ENABLE_TEST=ON)")
endif()