#pragma once

#include "hft/book/tick_scale.hpp"
#include "hft/book/top_of_book.hpp"
#include "hft/core/types.hpp"

#include <OrderBook.h>

#include <cstdint>

namespace hft::book
{

/* Drives a limit order book from a top-of-book snapshot feed.

   bookTicker sends state, not events: each frame is the current best bid and
   offer, with no order identities. A matching engine needs orders, so each
   snapshot is applied as a replace — cancel the two synthetic quotes resting from
   the previous frame, insert quotes at the new prices and sizes. The book then
   holds a two-level view a strategy or a fill simulator can match against, and
   top of book is tracked alongside it for readers that only need the BBO.

   Swapping in a depth or order-by-order feed means replacing this class, not the
   consumers reading TopOfBook.

   The pinned book revision allocates orders from a slab, so apply() does not
   touch the heap after startup. Measure it with bench/book_bench before putting
   it in a latency budget. */
class SnapshotBook
{
  public:
    explicit SnapshotBook(TickScale scale) : scale_(scale) {}

    /* Applies one snapshot. Ignores a frame whose update id is not newer than the
       last applied, which is a duplicate or a reordered delivery.
       Throws std::domain_error when a price or size is not a finite non-negative
       number. */
    void apply(const Message& msg);

    const TopOfBook&            top() const { return top_; }
    const orderbook::OrderBook& book() const { return book_; }

    std::uint64_t applied_count() const { return applied_count_; }
    std::uint64_t stale_count() const { return stale_count_; }
    std::uint64_t crossed_count() const { return crossed_count_; }

    /* Snapshots whose synthetic quotes matched instead of resting, which only
       happens on a crossed frame. */
    std::uint64_t trade_count() const { return trade_count_; }

    /* Quotes that had already left the book when the next snapshot tried to
       cancel them, meaning they were filled by a crossing frame. */
    std::uint64_t filled_quote_count() const { return filled_quote_count_; }

  private:
    void cancel_resting();
    void rest_quote(orderbook::Side side, std::int64_t price_ticks, std::uint64_t units,
                    std::uint64_t timestamp_ns);

    TickScale            scale_;
    TopOfBook            top_{};
    orderbook::OrderBook book_{};

    std::uint64_t      next_order_id_ = 1;
    orderbook::OrderId bid_id_{};
    orderbook::OrderId ask_id_{};
    bool               bid_resting_ = false;
    bool               ask_resting_ = false;

    std::uint64_t last_update_id_     = 0;
    std::uint64_t applied_count_      = 0;
    std::uint64_t stale_count_        = 0;
    std::uint64_t crossed_count_      = 0;
    std::uint64_t trade_count_        = 0;
    std::uint64_t filled_quote_count_ = 0;
};

}  // namespace hft::book
