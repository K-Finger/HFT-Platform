cmake_minimum_required(VERSION 3.20)
project(hft_platform CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

add_executable(ingestion
    src/ingestion.cpp
    src/shm_provider.cpp
)
target_include_directories(ingestion PRIVATE include)
target_link_libraries(ingestion PRIVATE
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread rt
)

add_executable(producer
    src/producer.cpp
    src/shm_provider.cpp
)

add_executable(consumer
    src/consumer.cpp
    src/shm_provider.cpp
)

foreach(target producer consumer)
    target_include_directories(${target} PRIVATE include)
    target_link_libraries(${target} PRIVATE pthread rt)
endforeach()
