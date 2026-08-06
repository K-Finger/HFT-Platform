#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "hft/core/types.hpp"

namespace hft::ipc
{

/* Lock-free single-producer single-consumer ring sized to live inside a shared
   mapping: storage is inline, nothing allocates, nothing locks. The producer
   owns head, the consumer owns tail, and each cursor gets its own cache line so
   the two cores never invalidate each other's line. */
struct SpscRing
{
    static constexpr std::size_t kCapacity = 1024;

    Message slots[kCapacity];

    alignas(64) std::atomic<std::uint64_t> tail;
    alignas(64) std::atomic<std::uint64_t> head;

    bool push(const Message& msg)
    {
        const std::uint64_t h = head.load(std::memory_order_relaxed);
        const std::uint64_t t = tail.load(std::memory_order_acquire);
        if ((h - t) >= kCapacity)
            return false;

        slots[h % kCapacity] = msg;
        head.store(h + 1, std::memory_order_release);  // slot write lands before the cursor moves
        return true;
    }

    bool pop(Message& out)
    {
        const std::uint64_t h = head.load(std::memory_order_acquire);
        const std::uint64_t t = tail.load(std::memory_order_relaxed);
        if (h == t)
            return false;

        out = slots[t % kCapacity];
        tail.store(t + 1, std::memory_order_release);  // slot frees only after the copy completes
        return true;
    }
};

}  // namespace hft::ipc
