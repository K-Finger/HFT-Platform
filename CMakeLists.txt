cmake_minimum_required(VERSION 3.20)
project(hft_platform CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED)
add_compile_definitions(BOOST_ASIO_NO_DEPRECATED)
find_package(OpenSSL REQUIRED)

# ── Order book submodule ──────────────────────────────────────────────────────
add_library(order_book STATIC
    lib/order-book/OrderBook.cpp
)
target_include_directories(order_book PUBLIC lib/order-book)
target_compile_features(order_book PUBLIC cxx_std_20)

# ── Ingestion ─────────────────────────────────────────────────────────────────
add_executable(ingestion
    src/ingestion.cpp
    src/shm_provider.cpp
)
target_include_directories(ingestion PRIVATE include include/core include/feed include/utils)
target_link_libraries(ingestion PRIVATE
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread rt
)

# ── Consumer ──────────────────────────────────────────────────────────────────
add_executable(consumer
    src/consumer.cpp
    src/shm_provider.cpp
)
target_include_directories(consumer PRIVATE include include/core include/feed include/utils include/strategy)
target_link_libraries(consumer PRIVATE order_book pthread rt)
