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
        uint64_t t = tail.load(std::memory_order_relaxed);
        if ((h - t) < 1024) {
            slots[h % 1024] = msg;
            head.store(h + 1, std::memory_order_release); // all writes before completed. 'im done' i can be read.
            return true;
        }
        return false;
    }

    bool pop(Message& out) {
        uint64_t h = head.load(std::memory_order_acquire);
        uint64_t t = tail.load(std::memory_order_relaxed);
        if (h == t) return false; // empty
        out = slots[t % 1024];
        tail.store(t + 1, std::memory_order_release);
        return true;
    }
};

#endif