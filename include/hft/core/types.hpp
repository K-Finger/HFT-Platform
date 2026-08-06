#pragma once

#include <cstdint>

namespace hft
{

/* Top of book snapshot crossing the shared memory seam. One message fills one
   cache line, so neighbouring slots never share a line. Pointer-free POD, which
   is what makes it valid to place inside a mapping another process reads. */
struct alignas(64) Message
{
    double        bid_price;
    double        ask_price;
    double        bid_qty;
    double        ask_qty;
    std::uint64_t timestamp;  // local receive time, nanoseconds
    std::uint64_t update_id;  // exchange sequence number, a gap means a dropped message
};

}  // namespace hft
