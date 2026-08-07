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

    std::uint64_t total = 0;
    std::uint64_t counts[kBuckets] = {};  // bucket i holds samples in [2^i, 2^(i+1))

    void record(std::uint64_t cycles)
    {
        const int bucket = cycles ? 63 - __builtin_clzll(cycles) : 0;
        counts[bucket]++;
        total++;
    }

    /* Bucket upper bound containing the p-th percentile sample. Resolution is
       one power of two, which is enough to spot a regression without storing
       every sample. */
    [[nodiscard]] std::uint64_t percentile(double p) const
    {
        const auto target = static_cast<std::uint64_t>(p * static_cast<double>(total));
        std::uint64_t cumulative = 0;
        for (int i = 0; i < kBuckets; i++)
        {
            cumulative += counts[i];
            if (cumulative > target)
                return 1ULL << (i + 1);
        }
        return 0;
    }

    void print(const char* unit = "cycles") const
    {
        std::printf("%-9s(<) %10s\n", unit, "count");
        for (int i = 0; i < kBuckets; i++)
        {
            if (counts[i] == 0)
                continue;
            std::printf("%-12llu %10llu\n", 1ULL << (i + 1),
                        static_cast<unsigned long long>(counts[i]));
        }
        std::printf("p50=%llu p99=%llu p999=%llu (%s)\n",
                    static_cast<unsigned long long>(percentile(0.50)),
                    static_cast<unsigned long long>(percentile(0.99)),
                    static_cast<unsigned long long>(percentile(0.999)), unit);
    }
};

}  // namespace hft::time
