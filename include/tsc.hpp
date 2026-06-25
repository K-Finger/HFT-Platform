/*  The Time Stamp Counter (TSC) is a 64-bit register
    on all x86 processors. It counts CPU cycles.
    This is used to profile our code performance. */

#pragma once
#include <stdint.h>
#include <cstdio>

struct LatencyHistogram
{
    uint64_t total = 0;  // sample count
    int counts[64] = {}; // buckets 2^i to 2^(i+1)

    void print()
    {
        printf("%-12s %10s\n", "cycles(<)", "count");
        for (int i = 0; i < 64; i++)
        {
            if (counts[i] == 0)
                continue;
            printf("%-12llu %10d\n", 1ULL << (i + 1), counts[i]);
        }
    }

    void record(uint64_t cycles)
    {
        total++;
        int bucket = cycles ? 63 - __builtin_clzll(cycles) : 0; // floor(log2(cycles))
        counts[bucket]++;
    };
};

/* Read Time Stamp Counter and return # of cycles */
inline uint64_t
rdtsc()
{
    uint64_t cycles;
    asm volatile(
        "rdtsc;"
        "shlq $32, %%rdx;"
        "orq %%rdx, %%rax;"
        : "=a"(cycles)
        :
        : "rdx");
    return cycles;
}
