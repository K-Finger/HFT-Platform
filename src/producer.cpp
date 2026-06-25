#include <iostream>
#include <cstdlib>
#include <chrono>

#include "shm_ring.hpp"
#include "shm_provider.hpp"

int main() {
    RingBuffer* ring = open_shared_memory(true);
    if (ring == nullptr) exit(1);

    std::cout << "Producer ready";

    for (int i = 0; i < 10; ++i) {
        unsigned int rand;
        __asm__("rdrand %0" : "=r"(rand));         
        bool side = rand & 1;
        double price = rand % 10'000;
        std::uint32_t qty = (rand / 2) % 100;
        auto now = std::chrono::high_resolution_clock::now();
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();

        std::cout << price << "\n" << side << "\n" << qty << "\n" << timestamp << "\n=========\n";

        Message msg = {
            .price = price,
            .qty = qty,
            .side = side,
            .timestamp = timestamp
        };

        ring->push(msg);
    }
    
    return 0;
}