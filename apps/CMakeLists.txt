add_executable(ingestion ingestion/main.cpp)
target_link_libraries(ingestion PRIVATE hft::feed hft::ipc)
hft_apply_target_options(ingestion)

add_executable(consumer consumer/main.cpp)
target_link_libraries(consumer PRIVATE hft::ipc orderbook)
hft_apply_target_options(consumer)
