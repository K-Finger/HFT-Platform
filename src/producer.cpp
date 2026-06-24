#include <iostream>
#include <cstdlib>

#include "shm_provider.hpp"

int main() {
    RingBuffer* ring = open_shared_memory(true);
    if (ring == nullptr) exit(1);

    std::cout << "Producer ready";
}