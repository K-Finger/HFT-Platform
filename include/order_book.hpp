#pragma once
#include <cstdint>
#include <cstdio>

#include "types.hpp"

struct OrderBook
{
    double best_bid_price = 0.0;
    double best_ask_price = 0.0;
    double best_bid_qty = 0.0;
    double best_ask_qty = 0.0;
    uint64_t last_update_id = 0;
    uint64_t tick_count = 0;

    bool update(const Message &msg)
    {
        if (msg.update_id <= last_update_id) [[unlikely]]
            return false;

        best_bid_price = msg.bid_price;
        best_bid_qty = msg.bid_qty;
        best_ask_price = msg.ask_price;
        best_ask_qty = msg.ask_qty;
        last_update_id = msg.update_id;
        tick_count++;
        return true;
    }

    double mid_price() const { return (best_bid_price + best_ask_price) / 2.0; }
    double spread() const { return best_ask_price - best_bid_price; }
    double spread_bps() const { return (spread() / mid_price()) * 10'000; }
    double imbalance() const { return (best_bid_qty - best_ask_qty) / (best_bid_qty + best_ask_qty); }
    bool is_crossed() const { return best_bid_price >= best_ask_price; }

    void print() const
    {
        printf("mid=%.2f spread=%.2f spread_bps=%.2f imbalance=%.4f ticks=%llu\n",
               mid_price(), spread(), spread_bps(), imbalance(), tick_count);
    }
};
