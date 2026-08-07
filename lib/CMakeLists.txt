# Third-party sources vendored as git submodules, see .gitmodules.

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/order-book/CMakeLists.txt)
    message(FATAL_ERROR
        "lib/order-book is empty. Fetch the submodule first: "
        "git submodule update --init --recursive")
endif()

# The submodule ships its own build. Adding it as a subdirectory gives us the
# orderbook target with its include directories already attached; its tests and
# benchmarks stay off because they are guarded behind a top-level check upstream.
add_subdirectory(order-book EXCLUDE_FROM_ALL)
