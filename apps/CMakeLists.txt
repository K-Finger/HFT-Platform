add_executable(ingestion ingestion/main.cpp)
target_link_libraries(ingestion PRIVATE hft::feed hft::ipc)
hft_apply_target_options(ingestion)

add_executable(consumer consumer/main.cpp)
target_link_libraries(consumer PRIVATE hft::ipc hft::book)
hft_apply_target_options(consumer)

add_executable(recorder recorder/main.cpp)
target_link_libraries(recorder PRIVATE hft::feed hft::capture)
hft_apply_target_options(recorder)

add_executable(replay replay/main.cpp)
target_link_libraries(replay PRIVATE hft::capture hft::feed hft::ipc)
hft_apply_target_options(replay)
