#pragma once

#include <cstdint>
#include <cstdio>

namespace hft::time
{

/* Cycle counts bucketed by power of two. Recording costs a count-leading-zeros
   and two increments, so it can sit inside the loop it measures. */
struct LatencyHistogram
{
    static constexpr int kBuckets = 64;

    std::uint64_t total            = 0;
    std::uint64_t counts[kBuckets] = {};  // bucket i holds samples in [2^i, 2^(i+1))

    void record(std::uint64_t cycles)
    {
        const int bucket = cycles ? 63 - __builtin_clzll(cycles) : 0;
        counts[bucket]++;
        total++;
    }

    void print() const
    {
        std::printf("%-12s %10s\n", "cycles(<)", "count");
        for (int i = 0; i < kBuckets; i++)
        {
            if (counts[i] == 0)
                continue;
            std::printf("%-12llu %10llu\n", 1ULL << (i + 1),
                        static_cast<unsigned long long>(counts[i]));
        }
    }
};

}  // namespace hft::time
