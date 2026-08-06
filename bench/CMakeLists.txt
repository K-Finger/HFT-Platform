include(FetchContent)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.0
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

set(HFT_BENCH_SOURCES
    spsc_ring_bench.cpp
    tsc_bench.cpp
    book_ticker_parser_bench.cpp
)

foreach(bench_source ${HFT_BENCH_SOURCES})
    get_filename_component(bench_name ${bench_source} NAME_WE)
    add_executable(${bench_name} ${bench_source})
    target_link_libraries(${bench_name} PRIVATE hft::feed hft::ipc benchmark::benchmark_main)
    target_include_directories(${bench_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    hft_apply_target_options(${bench_name})
endforeach()
