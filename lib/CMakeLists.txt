# Third-party sources vendored as git submodules, see .gitmodules.

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/order-book/OrderBook.cpp)
    message(FATAL_ERROR
        "lib/order-book is empty. Fetch the submodule first: "
        "git submodule update --init --recursive")
endif()

add_library(orderbook STATIC
    order-book/OrderBook.cpp
)

target_include_directories(orderbook PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/order-book>
    $<INSTALL_INTERFACE:include/orderbook>
)
target_compile_features(orderbook PUBLIC cxx_std_20)

install(DIRECTORY order-book/
    DESTINATION include/orderbook
    FILES_MATCHING PATTERN "*.h"
)
