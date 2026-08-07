#pragma once

#include <cstdint>
#include <stdexcept>

namespace hft::book
{

/* Best bid and offer in integer ticks and size units. Held separately from the
   order book because the pinned book revision exposes no depth query, and because
   a strategy reading top of book should not walk a map to get it. */
struct TopOfBook
{
    std::int64_t bid_ticks = 0;
    std::int64_t ask_ticks = 0;
    std::uint64_t bid_units = 0;
    std::uint64_t ask_units = 0;

    [[nodiscard]] std::int64_t spread_ticks() const { return ask_ticks - bid_ticks; }

    /* True when the snapshot quotes an ask at or below the bid. Real venues do
       not, so this flags a stale or interleaved update. */
    [[nodiscard]] bool crossed() const { return ask_ticks <= bid_ticks; }

    [[nodiscard]] double mid_ticks() const
    {
        return (static_cast<double>(bid_ticks) + static_cast<double>(ask_ticks)) / 2.0;
    }

    /* Size-weighted mid: the side with more resting size pulls the fair price
       toward the other side's quote, because that side is likelier to trade next.
       Throws std::domain_error when neither side shows size, since no fair price
       exists to report. */
    [[nodiscard]] double microprice_ticks() const
    {
        const std::uint64_t total_units = bid_units + ask_units;
        if (total_units == 0)
            throw std::domain_error("microprice is undefined with no size on either side");

        return (static_cast<double>(bid_ticks) * static_cast<double>(ask_units) +
                static_cast<double>(ask_ticks) * static_cast<double>(bid_units)) /
               static_cast<double>(total_units);
    }
};

}  // namespace hft::book
