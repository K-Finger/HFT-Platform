cmake_minimum_required(VERSION 3.20)
project(hft_platform CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED)
add_compile_definitions(BOOST_ASIO_NO_DEPRECATED)
find_package(OpenSSL REQUIRED)

add_executable(ingestion
    src/ingestion.cpp
    src/shm_provider.cpp
)
target_include_directories(ingestion PRIVATE include)
target_link_libraries(ingestion PRIVATE
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread rt
)

add_executable(consumer
    src/consumer.cpp
    src/shm_provider.cpp
)

target_include_directories(consumer PRIVATE include)
target_link_libraries(consumer PRIVATE pthread rt)
