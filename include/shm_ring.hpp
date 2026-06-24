#ifndef SHM_RING_HPP
#define SHM_RING_HPP

#include <cstdint>
#include <atomic>

struct alignas(64) Message
{
    double price;
    std::uint32_t qty;
    std::uint8_t side; // buy = 0, sell = 1
    std::uint64_t timestamp; // nanoseconds
};

struct RingBuffer
{
    Message slots[1024];
    std::atomic<std::uint64_t> tail; // read
    std::atomic<std::uint64_t> head; // write

    bool push(const Message& msg) {
        uint64_t h = head.load(std::memory_order_relaxed);
        if ((head - tail) < 1024) {
            slots[head % 1024] = msg;
            head.store(h + 1, std::memory_order_release)
        }
    }
};

#endif