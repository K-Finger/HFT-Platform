#include "hft/book/snapshot_book.hpp"

#include <Order.h>
#include <OrderType.h>
#include <Side.h>

#include <chrono>
#include <span>

namespace hft::book
{
namespace
{
using orderbook::Order;
using orderbook::OrderId;
using orderbook::OrderType;
using orderbook::Price;
using orderbook::Quantity;
using orderbook::Side;
using orderbook::Timestamp;
using orderbook::Trade;
}  // namespace

void SnapshotBook::apply(const Message& msg)
{
    if (applied_count_ != 0 && msg.update_id <= last_update_id_)
    {
        stale_count_++;
        return;
    }

    const TopOfBook next{scale_.price_ticks(msg.bid_price), scale_.price_ticks(msg.ask_price),
                         scale_.quantity_units(msg.bid_qty), scale_.quantity_units(msg.ask_qty)};

    cancel_resting();

    // A side quoting zero size has nothing to rest, so leave the book empty there
    // rather than inserting an order that can never trade.
    if (next.bid_units != 0)
        rest_quote(Side::Buy, next.bid_ticks, next.bid_units, msg.timestamp);
    if (next.ask_units != 0)
        rest_quote(Side::Sell, next.ask_ticks, next.ask_units, msg.timestamp);

    if (next.crossed())
        crossed_count_++;

    top_ = next;
    last_update_id_ = msg.update_id;
    applied_count_++;
}

void SnapshotBook::cancel_resting()
{
    // A cancel fails when a crossing frame already matched the quote, so the
    // failure is a fill signal rather than an error.
    if (bid_resting_ && !book_.cancelOrder(bid_id_))
        filled_quote_count_++;
    if (ask_resting_ && !book_.cancelOrder(ask_id_))
        filled_quote_count_++;

    bid_resting_ = false;
    ask_resting_ = false;
}

void SnapshotBook::rest_quote(Side side, std::int64_t price_ticks, std::uint64_t units,
                              std::uint64_t timestamp_ns)
{
    const OrderId id{next_order_id_++};

    // The span views a buffer the book reuses, so it stays valid only until the
    // next addOrder. It is consumed here, before any further call.
    const std::span<const Trade> trades =
        book_.addOrder(Order{OrderType::GoodTillCancel, id, side, Price{price_ticks},
                             Quantity{units}, Timestamp{std::chrono::nanoseconds{timestamp_ns}}});
    trade_count_ += trades.size();

    // Every returned trade involves the order just added, so its filled quantity
    // is their sum. A good-till-cancel remainder rests and must be cancelled by
    // the next snapshot; a full fill left the book already.
    std::uint64_t filled_units = 0;
    for (const Trade& trade : trades)
        filled_units += trade.quantity.get();

    const bool rests = filled_units < units;

    if (side == Side::Buy)
    {
        bid_id_ = id;
        bid_resting_ = rests;
    }
    else
    {
        ask_id_ = id;
        ask_resting_ = rests;
    }
}

}  // namespace hft::book
