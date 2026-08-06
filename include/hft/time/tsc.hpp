#pragma once

#include <cstdint>

namespace hft::time
{

/* Reads the x86 cycle counter. The cheapest clock available and the only one
   fine grained enough to measure the hot path. Not comparable across cores. */
inline std::uint64_t rdtsc()
{
    std::uint64_t cycles;
    asm volatile("rdtsc;"
                 "shlq $32, %%rdx;"
                 "orq %%rdx, %%rax;"
                 : "=a"(cycles)
                 :
                 : "rdx");
    return cycles;
}

}  // namespace hft::time
