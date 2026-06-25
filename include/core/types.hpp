#pragma once

#include <cstdint>

struct alignas(64) Message
{
    double   bid_price;
    double   ask_price;
    double   bid_qty;
    double   ask_qty;
    uint64_t timestamp;   // local receive time, nanoseconds
    uint64_t update_id;   // binance sequence number — gap = dropped message
};
