# One test binary per library module, so a failure points at one module.

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(hft_core_tests
    latency_histogram_tests.cpp
)
target_link_libraries(hft_core_tests PRIVATE hft::core GTest::gtest_main)
gtest_discover_tests(hft_core_tests)

add_executable(hft_ipc_tests
    spsc_ring_tests.cpp
    shared_memory_tests.cpp
)
target_link_libraries(hft_ipc_tests PRIVATE hft::ipc GTest::gtest_main)
gtest_discover_tests(hft_ipc_tests)

add_executable(hft_feed_tests
    book_ticker_parser_tests.cpp
)
target_link_libraries(hft_feed_tests PRIVATE hft::feed GTest::gtest_main)
gtest_discover_tests(hft_feed_tests)
