#include <iostream>
#include <cstdlib>
#include <atomic>

#include "shm_ring.hpp"
#include "shm_provider.hpp"
#include "thread_utils.hpp"

int main() {
    pin_thread_to_core(3); // consumer core
    set_realtime(80); // high prio

    RingBuffer* ring = open_shared_memory(false);
    Message msg;
    int received = 0;
    while (received < 10) {
        Message msg;
        if (ring->pop(msg)) {
            std::cout << msg.price << "\n";
            received++;
        }
    }
}
