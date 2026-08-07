#pragma once

#include <cerrno>
#include <cstdint>
#include <ctime>
#include <system_error>

namespace hft::time
{

/* Wall clock nanoseconds for receive timestamps. Comparable across processes,
   unlike the TSC, and roughly 20ns more expensive to read. */
inline std::uint64_t now_ns()
{
    timespec ts{};
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        throw std::system_error(errno, std::generic_category(),
                                "clock_gettime(CLOCK_REALTIME) failed");

    return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
           static_cast<std::uint64_t>(ts.tv_nsec);
}

}  // namespace hft::time
