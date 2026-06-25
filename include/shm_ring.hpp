#ifndef SHM_RING_HPP
#define SHM_RING_HPP

#include <cstdint>
#include <cstddef>
#include <atomic>

struct alignas(64) Message
{
    double bid_price;
    double ask_price;
    double bid_qty;
    double ask_qty;
    std::uint64_t timestamp;  // local receive time, nanoseconds
    std::uint64_t update_id;  // binance sequence number — gap = dropped message
};

static_assert(sizeof(Message) == 64, "Message must be 64 bytes");
static_assert(offsetof(Message, price) == 0, "price offset wrong");
static_assert(offsetof(Message, qty) == 8, "qty offset wrong");
static_assert(offsetof(Message, side) == 12, "side offset wrong");
static_assert(offsetof(Message, timestamp) == 16, "timestamp offset wrong");

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