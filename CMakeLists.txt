cmake_minimum_required(VERSION 3.20)
project(hft
    VERSION 0.1.0
    DESCRIPTION "Modular low-latency market data ingestion and IPC library"
    LANGUAGES CXX
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(HFT_TOP_LEVEL ON)
else()
    set(HFT_TOP_LEVEL OFF)
endif()

option(HFT_BUILD_APPS "Build the ingestion and consumer executables" ${HFT_TOP_LEVEL})
option(HFT_BUILD_TESTS "Build the unit tests" ${HFT_TOP_LEVEL})
option(HFT_BUILD_BENCH "Build the microbenchmarks" ${HFT_TOP_LEVEL})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT HFT_IPO_SUPPORTED)

include(cmake/hftTargetOptions.cmake)

# ── hft::core — header-only types, clocks and OS primitives ───────────────────
add_library(hft_core INTERFACE)
add_library(hft::core ALIAS hft_core)

target_include_directories(hft_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(hft_core INTERFACE cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(hft_core INTERFACE Threads::Threads)

# ── hft::ipc — shared memory SPSC ring ───────────────────────────────────────
add_library(hft_ipc STATIC
    src/ipc/shared_memory.cpp
)
add_library(hft::ipc ALIAS hft_ipc)

target_link_libraries(hft_ipc PUBLIC hft_core rt)
hft_apply_target_options(hft_ipc)

# ── hft::capture — capture file writer and mmap reader ───────────────────────
add_library(hft_capture STATIC
    src/capture/capture_reader.cpp
    src/capture/capture_writer.cpp
)
add_library(hft::capture ALIAS hft_capture)

target_link_libraries(hft_capture PUBLIC hft_core)
hft_apply_target_options(hft_capture)

# ── hft::feed — exchange websocket client and wire parsers ───────────────────
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(hft_feed STATIC
    src/feed/book_ticker_parser.cpp
    src/feed/websocket_feed.cpp
)
add_library(hft::feed ALIAS hft_feed)

target_link_libraries(hft_feed PUBLIC
    hft_core
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
)
target_compile_definitions(hft_feed PUBLIC BOOST_ASIO_NO_DEPRECATED)
hft_apply_target_options(hft_feed)

set(HFT_LIBRARY_TARGETS hft_core hft_ipc hft_capture hft_feed)

if(HFT_BUILD_APPS)
    add_subdirectory(apps)
endif()

if(HFT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HFT_BUILD_BENCH)
    add_subdirectory(bench)
endif()

# ── Install and export as the hft:: package ──────────────────────────────────
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS ${HFT_LIBRARY_TARGETS}
    EXPORT hftTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/hft
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT hftTargets
    FILE hftTargets.cmake
    NAMESPACE hft::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hft
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/hftConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_file(cmake/hftConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/hftConfig.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/hftConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/hftConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hft
)
